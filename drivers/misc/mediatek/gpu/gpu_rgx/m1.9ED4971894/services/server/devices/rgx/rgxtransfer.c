/*************************************************************************/ /*!
@File
@Title          Device specific transfer queue routines
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
#include "rgxtransfer.h"
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
#include "rgx_bvnc_defs_km.h"

#if defined(SUPPORT_BUFFER_SYNC)
#include "pvr_buffer_sync.h"
#endif

#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
#include "sync_checkpoint.h"
#include "sync_checkpoint_internal.h"
#endif
#if defined(SUPPORT_NATIVE_FENCE_SYNC)
#include "pvr_sync.h"
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) */

/* Enable this to dump the compiled list of UFOs prior to kick call */
#define ENABLE_TQ_UFO_DUMP	0

//#define TRANSFER_CHECKPOINT_DEBUG 1

#if defined(TRANSFER_CHECKPOINT_DEBUG)
#define CHKPT_DBG(X) PVR_DPF(X)
#else
#define CHKPT_DBG(X)
#endif

typedef struct {
	DEVMEM_MEMDESC				*psFWContextStateMemDesc;
	RGX_SERVER_COMMON_CONTEXT	*psServerCommonContext;
	IMG_UINT32					ui32Priority;
} RGX_SERVER_TQ_3D_DATA;


typedef struct {
	RGX_SERVER_COMMON_CONTEXT	*psServerCommonContext;
	IMG_UINT32					ui32Priority;
} RGX_SERVER_TQ_2D_DATA;

struct _RGX_SERVER_TQ_CONTEXT_ {
	PVRSRV_DEVICE_NODE			*psDeviceNode;
	DEVMEM_MEMDESC				*psFWFrameworkMemDesc;
	IMG_UINT32					ui32Flags;
#define RGX_SERVER_TQ_CONTEXT_FLAGS_2D		(1<<0)
#define RGX_SERVER_TQ_CONTEXT_FLAGS_3D		(1<<1)
	RGX_SERVER_TQ_3D_DATA		s3DData;
	RGX_SERVER_TQ_2D_DATA		s2DData;
	PVRSRV_CLIENT_SYNC_PRIM		*psCleanupSync;
	DLLIST_NODE					sListNode;
	ATOMIC_T			hJobId;
	IMG_UINT32			ui32PDumpFlags;
	/* per-prepare sync address lists */
	SYNC_ADDR_LIST			asSyncAddrListFence[TQ_MAX_PREPARES_PER_SUBMIT];
	SYNC_ADDR_LIST			asSyncAddrListUpdate[TQ_MAX_PREPARES_PER_SUBMIT];
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	POS_LOCK				hLock;
#endif
};

/*
	Static functions used by transfer context code
*/
static PVRSRV_ERROR _Create3DTransferContext(CONNECTION_DATA *psConnection,
											 PVRSRV_DEVICE_NODE *psDeviceNode,
											 DEVMEM_MEMDESC *psFWMemContextMemDesc,
											 IMG_UINT32 ui32Priority,
											 RGX_COMMON_CONTEXT_INFO *psInfo,
											 RGX_SERVER_TQ_3D_DATA *ps3DData)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR eError;

	/*
		Allocate device memory for the firmware GPU context suspend state.
		Note: the FW reads/writes the state to memory by accessing the GPU register interface.
	*/
	PDUMPCOMMENT("Allocate RGX firmware TQ/3D context suspend state");

	eError = DevmemFwAllocate(psDevInfo,
							sizeof(RGXFWIF_3DCTX_STATE),
							RGX_FWCOMCTX_ALLOCFLAGS,
							"FwTQ3DContext",
							&ps3DData->psFWContextStateMemDesc);
	if (eError != PVRSRV_OK)
	{
		goto fail_contextswitchstate;
	}

	eError = FWCommonContextAllocate(psConnection,
									 psDeviceNode,
									 REQ_TYPE_TQ_3D,
									 RGXFWIF_DM_3D,
									 NULL,
									 0,
									 psFWMemContextMemDesc,
									 ps3DData->psFWContextStateMemDesc,
									 RGX_TQ3D_CCB_SIZE_LOG2,
									 ui32Priority,
									 psInfo,
									 &ps3DData->psServerCommonContext);
	if (eError != PVRSRV_OK)
	{
		goto fail_contextalloc;
	}


	PDUMPCOMMENT("Dump 3D context suspend state buffer");
	DevmemPDumpLoadMem(ps3DData->psFWContextStateMemDesc, 0, sizeof(RGXFWIF_3DCTX_STATE), PDUMP_FLAGS_CONTINUOUS);

	ps3DData->ui32Priority = ui32Priority;
	return PVRSRV_OK;

fail_contextalloc:
	DevmemFwFree(psDevInfo, ps3DData->psFWContextStateMemDesc);
fail_contextswitchstate:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}


static PVRSRV_ERROR _Create2DTransferContext(CONNECTION_DATA *psConnection,
											 PVRSRV_DEVICE_NODE *psDeviceNode,
											 DEVMEM_MEMDESC *psFWMemContextMemDesc,
											 IMG_UINT32 ui32Priority,
											 RGX_COMMON_CONTEXT_INFO *psInfo,
											 RGX_SERVER_TQ_2D_DATA *ps2DData)
{
	PVRSRV_ERROR eError;

	eError = FWCommonContextAllocate(psConnection,
									 psDeviceNode,
									 REQ_TYPE_TQ_2D,
									 RGXFWIF_DM_2D,
									 NULL,
									 0,
									 psFWMemContextMemDesc,
									 NULL,
									 RGX_TQ2D_CCB_SIZE_LOG2,
									 ui32Priority,
									 psInfo,
									 &ps2DData->psServerCommonContext);
	if (eError != PVRSRV_OK)
	{
		goto fail_contextalloc;
	}

	ps2DData->ui32Priority = ui32Priority;
	return PVRSRV_OK;

fail_contextalloc:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}


static PVRSRV_ERROR _Destroy2DTransferContext(RGX_SERVER_TQ_2D_DATA *ps2DData,
											  PVRSRV_DEVICE_NODE *psDeviceNode,
											  PVRSRV_CLIENT_SYNC_PRIM *psCleanupSync,
											  IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eError;

	/* Check if the FW has finished with this resource ... */
	eError = RGXFWRequestCommonContextCleanUp(psDeviceNode,
											  ps2DData->psServerCommonContext,
											  psCleanupSync,
											  RGXFWIF_DM_2D,
											  ui32PDumpFlags);
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
	FWCommonContextFree(ps2DData->psServerCommonContext);
	ps2DData->psServerCommonContext = NULL;
	return PVRSRV_OK;
}

static PVRSRV_ERROR _Destroy3DTransferContext(RGX_SERVER_TQ_3D_DATA *ps3DData,
											  PVRSRV_DEVICE_NODE *psDeviceNode,
											  PVRSRV_CLIENT_SYNC_PRIM *psCleanupSync,
											  IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eError;

	/* Check if the FW has finished with this resource ... */
	eError = RGXFWRequestCommonContextCleanUp(psDeviceNode,
											  ps3DData->psServerCommonContext,
											  psCleanupSync,
											  RGXFWIF_DM_3D,
											  ui32PDumpFlags);
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
	DevmemFwFree(psDeviceNode->pvDevice, ps3DData->psFWContextStateMemDesc);
	FWCommonContextFree(ps3DData->psServerCommonContext);
	ps3DData->psServerCommonContext = NULL;
	return PVRSRV_OK;
}


/*
 * PVRSRVCreateTransferContextKM
 */
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXCreateTransferContextKM(CONNECTION_DATA		*psConnection,
										   PVRSRV_DEVICE_NODE		*psDeviceNode,
										   IMG_UINT32				ui32Priority,
										   IMG_UINT32				ui32FrameworkCommandSize,
										   IMG_PBYTE				pabyFrameworkCommand,
										   IMG_HANDLE				hMemCtxPrivData,
										   RGX_SERVER_TQ_CONTEXT	**ppsTransferContext)
{
	RGX_SERVER_TQ_CONTEXT	*psTransferContext;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	DEVMEM_MEMDESC			*psFWMemContextMemDesc = RGXGetFWMemDescFromMemoryContextHandle(hMemCtxPrivData);
	RGX_COMMON_CONTEXT_INFO	sInfo;
	PVRSRV_ERROR			eError = PVRSRV_OK;

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
		goto fail_createlock;
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

	eError = _Create3DTransferContext(psConnection,
									  psDeviceNode,
									  psFWMemContextMemDesc,
									  ui32Priority,
									  &sInfo,
									  &psTransferContext->s3DData);
	if (eError != PVRSRV_OK)
	{
		goto fail_3dtransfercontext;
	}
	psTransferContext->ui32Flags |= RGX_SERVER_TQ_CONTEXT_FLAGS_3D;

	if(psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK)
	{
		eError = _Create2DTransferContext(psConnection,
										  psDeviceNode,
										  psFWMemContextMemDesc,
										  ui32Priority,
										  &sInfo,
										  &psTransferContext->s2DData);
		if (eError != PVRSRV_OK)
		{
			goto fail_2dtransfercontext;
		}
		psTransferContext->ui32Flags |= RGX_SERVER_TQ_CONTEXT_FLAGS_2D;
	}

	{
		PVRSRV_RGXDEV_INFO			*psDevInfo = psDeviceNode->pvDevice;

		OSWRLockAcquireWrite(psDevInfo->hTransferCtxListLock);
		dllist_add_to_tail(&(psDevInfo->sTransferCtxtListHead), &(psTransferContext->sListNode));
		OSWRLockReleaseWrite(psDevInfo->hTransferCtxListLock);
		*ppsTransferContext = psTransferContext;
	}

	*ppsTransferContext = psTransferContext;
	
	return PVRSRV_OK;


fail_2dtransfercontext:
	if(psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK)
	{
		_Destroy3DTransferContext(&psTransferContext->s3DData,
								  psTransferContext->psDeviceNode,
								  psTransferContext->psCleanupSync,
								  psTransferContext->ui32PDumpFlags);
	}

fail_3dtransfercontext:
fail_frameworkcopy:
	DevmemFwFree(psDevInfo, psTransferContext->psFWFrameworkMemDesc);
fail_frameworkcreate:
	SyncPrimFree(psTransferContext->psCleanupSync);
fail_syncalloc:
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psTransferContext->hLock);
fail_createlock:
#endif
	OSFreeMem(psTransferContext);
	PVR_ASSERT(eError != PVRSRV_OK);
	*ppsTransferContext = NULL;
	return eError;
}

IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXDestroyTransferContextKM(RGX_SERVER_TQ_CONTEXT *psTransferContext)
{
	PVRSRV_ERROR eError;
	PVRSRV_RGXDEV_INFO *psDevInfo = psTransferContext->psDeviceNode->pvDevice;
	IMG_UINT32 i;

	/* remove node from list before calling destroy - as destroy, if successful
	 * will invalidate the node
	 * must be re-added if destroy fails
	 */
	OSWRLockAcquireWrite(psDevInfo->hTransferCtxListLock);
	dllist_remove_node(&(psTransferContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hTransferCtxListLock);

	if ((psTransferContext->ui32Flags & RGX_SERVER_TQ_CONTEXT_FLAGS_2D) && \
			(psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK))
	{
		eError = _Destroy2DTransferContext(&psTransferContext->s2DData,
										   psTransferContext->psDeviceNode,
										   psTransferContext->psCleanupSync,
										   PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			goto fail_destroy2d;
		}
		/* We've freed the 2D context, don't try to free it again */
		psTransferContext->ui32Flags &= ~RGX_SERVER_TQ_CONTEXT_FLAGS_2D;
	}

	if (psTransferContext->ui32Flags & RGX_SERVER_TQ_CONTEXT_FLAGS_3D)
	{
		eError = _Destroy3DTransferContext(&psTransferContext->s3DData,
										   psTransferContext->psDeviceNode,
										   psTransferContext->psCleanupSync,
										   PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			goto fail_destroy3d;
		}
		/* We've freed the 3D context, don't try to free it again */
		psTransferContext->ui32Flags &= ~RGX_SERVER_TQ_CONTEXT_FLAGS_3D;
	}

	/* free any resources within the per-prepare UFO address stores */
	for(i = 0; i < TQ_MAX_PREPARES_PER_SUBMIT; i++)
	{
		SyncAddrListDeinit(&psTransferContext->asSyncAddrListFence[i]);
		SyncAddrListDeinit(&psTransferContext->asSyncAddrListUpdate[i]);
	}

	DevmemFwFree(psDevInfo, psTransferContext->psFWFrameworkMemDesc);
	SyncPrimFree(psTransferContext->psCleanupSync);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psTransferContext->hLock);
#endif

	OSFreeMem(psTransferContext);

	return PVRSRV_OK;

fail_destroy3d:

fail_destroy2d:
	OSWRLockAcquireWrite(psDevInfo->hTransferCtxListLock);
	dllist_add_to_tail(&(psDevInfo->sTransferCtxtListHead), &(psTransferContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hTransferCtxListLock);
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

/*
 * PVRSRVSubmitTQ3DKickKM
 */
IMG_EXPORT
PVRSRV_ERROR PVRSRVRGXSubmitTransferKM(RGX_SERVER_TQ_CONTEXT	*psTransferContext,
									   IMG_UINT32				ui32ClientCacheOpSeqNum,
									   IMG_UINT32				ui32PrepareCount,
									   IMG_UINT32				*paui32ClientFenceCount,
									   SYNC_PRIMITIVE_BLOCK		***papauiClientFenceUFOSyncPrimBlock,
									   IMG_UINT32				**papaui32ClientFenceSyncOffset,
									   IMG_UINT32				**papaui32ClientFenceValue,
									   IMG_UINT32				*paui32ClientUpdateCount,
									   SYNC_PRIMITIVE_BLOCK		***papauiClientUpdateUFOSyncPrimBlock,
									   IMG_UINT32				**papaui32ClientUpdateSyncOffset,
									   IMG_UINT32				**papaui32ClientUpdateValue,
									   IMG_UINT32				*paui32ServerSyncCount,
									   IMG_UINT32				**papaui32ServerSyncFlags,
									   SERVER_SYNC_PRIMITIVE	***papapsServerSyncs,
									   PVRSRV_FENCE				iCheckFence,
									   PVRSRV_TIMELINE			iUpdateTimeline,
									   PVRSRV_FENCE				*piUpdateFence,
									   IMG_CHAR					szFenceName[32],
									   IMG_UINT32				*paui32FWCommandSize,
									   IMG_UINT8				**papaui8FWCommand,
									   IMG_UINT32				*pui32TQPrepareFlags,
									   IMG_UINT32				ui32ExtJobRef,
									   IMG_UINT32				ui32SyncPMRCount,
									   IMG_UINT32				*paui32SyncPMRFlags,
									   PMR						**ppsSyncPMRs)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = psTransferContext->psDeviceNode;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGX_CCB_CMD_HELPER_DATA *pas3DCmdHelper;
	RGX_CCB_CMD_HELPER_DATA *pas2DCmdHelper;
	IMG_UINT32 ui323DCmdCount = 0;
	IMG_UINT32 ui322DCmdCount = 0;
	IMG_UINT32 ui323DCmdOffset = 0;
	IMG_UINT32 ui322DCmdOffset = 0;
	IMG_UINT32 ui32PDumpFlags = PDUMP_FLAGS_NONE;
	IMG_UINT32 i;
	IMG_UINT32 ui32IntClientFenceCount = 0;
	IMG_UINT32 *paui32IntFenceValue = NULL;
	IMG_UINT32 ui32IntClientUpdateCount = 0;
	IMG_UINT32 *paui32IntUpdateValue = NULL;
	SYNC_ADDR_LIST *psSyncAddrListFence;
	SYNC_ADDR_LIST *psSyncAddrListUpdate;
	IMG_UINT32               uiCheckFenceUID = 0;
	IMG_UINT32               uiUpdateFenceUID = 0;
#if defined(SUPPORT_NATIVE_FENCE_SYNC) && !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	struct pvr_sync_append_data *psFDFenceData = NULL;
#endif
#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	PSYNC_CHECKPOINT psUpdateSyncCheckpoint = NULL;
	PSYNC_CHECKPOINT *apsFenceSyncCheckpoints = NULL;
	IMG_UINT32 ui32FenceSyncCheckpointCount = 0;
	IMG_UINT32 *pui32IntAllocatedUpdateValues = NULL;
	PVRSRV_CLIENT_SYNC_PRIM *psFenceTimelineUpdateSync = NULL;
	IMG_UINT32 ui32FenceTimelineUpdateValue = 0;
	PSYNC_CHECKPOINT psBufferUpdateSyncCheckpoint = NULL;
	void *pvUpdateFenceFinaliseData = NULL;
#if defined(SUPPORT_BUFFER_SYNC)
	struct pvr_buffer_sync_append_data *psBufferSyncData = NULL;
	PSYNC_CHECKPOINT *apsBufferFenceSyncCheckpoints = NULL;
	IMG_UINT32 ui32BufferFenceSyncCheckpointCount = 0;
#endif /* defined(SUPPORT_BUFFER_SYNC) */
#endif /* defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
	PVRSRV_ERROR eError;
	PVRSRV_ERROR eError2;
	PVRSRV_FENCE iUpdateFence = PVRSRV_FENCE_INVALID;
	IMG_UINT32 ui32JobId;

	PRGXFWIF_TIMESTAMP_ADDR pPreAddr;
	PRGXFWIF_TIMESTAMP_ADDR pPostAddr;
	PRGXFWIF_UFO_ADDR       pRMWUFOAddr;

#if defined(SUPPORT_BUFFER_SYNC) && !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	struct pvr_buffer_sync_append_data *psAppendData = NULL;
#endif

	IMG_DEV_VIRTADDR sRobustnessResetReason = {0};

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
	if (iUpdateTimeline >= 0 && !piUpdateFence)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#else /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */
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
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */

	/* Ensure the string is null-terminated (Required for safety) */
	szFenceName[31] = '\0';

	if ((ui32PrepareCount == 0) || (ui32PrepareCount > TQ_MAX_PREPARES_PER_SUBMIT))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32SyncPMRCount != 0)
	{
		if (!ppsSyncPMRs)
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

#if defined(SUPPORT_BUFFER_SYNC)
		/* PMR sync is valid only when there is no batching */
		if ((ui32PrepareCount != 1))
#endif
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	if (iCheckFence >= 0 || iUpdateTimeline >= 0)
	{
#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
		/* Fence FD's are only valid in the 3D case with no batching */
		if ((ui32PrepareCount !=1) && (!TQ_PREP_FLAGS_COMMAND_IS(pui32TQPrepareFlags[0], 3D)))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
#else /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */
		/* Timelines/Fences are unsupported */
		return PVRSRV_ERROR_INVALID_PARAMS;
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psTransferContext->hLock);
#endif

	ui32JobId = OSAtomicIncrement(&psTransferContext->hJobId);

	/* We can't allocate the required amount of stack space on all consumer architectures */
	pas3DCmdHelper = OSAllocMem(sizeof(*pas3DCmdHelper) * ui32PrepareCount);
	if (pas3DCmdHelper == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto fail_alloc3dhelper;
	}
	pas2DCmdHelper = OSAllocMem(sizeof(*pas2DCmdHelper) * ui32PrepareCount);
	if (pas2DCmdHelper == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto fail_alloc2dhelper;
	}

	/*
		Ensure we do the right thing for server syncs which cross call boundaries
	*/
	for (i=0;i<ui32PrepareCount;i++)
	{
		IMG_BOOL bHaveStartPrepare = pui32TQPrepareFlags[i] & TQ_PREP_FLAGS_START;
		IMG_BOOL bHaveEndPrepare = IMG_FALSE;

		if (bHaveStartPrepare)
		{
			IMG_UINT32 k;
			/*
				We've at the start of a transfer operation (which might be made
				up of multiple HW operations) so check if we also have then
				end of the transfer operation in the batch
			*/
			for (k=i;k<ui32PrepareCount;k++)
			{
				if (pui32TQPrepareFlags[k] & TQ_PREP_FLAGS_END)
				{
					bHaveEndPrepare = IMG_TRUE;
					break;
				}
			}

			if (!bHaveEndPrepare)
			{
				/*
					We don't have the complete command passed in this call
					so drop the update request. When we get called again with
					the last HW command in this transfer operation we'll do
					the update at that point.
				*/
				for (k=0;k<paui32ServerSyncCount[i];k++)
				{
					papaui32ServerSyncFlags[i][k] &= ~PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE;
				}
			}
		}
	}


	/*
		Init the command helper commands for all the prepares
	*/
	for (i=0;i<ui32PrepareCount;i++)
	{
		RGX_CLIENT_CCB *psClientCCB;
		RGX_SERVER_COMMON_CONTEXT *psServerCommonCtx;
		IMG_CHAR *pszCommandName;
		RGX_CCB_CMD_HELPER_DATA *psCmdHelper;
		RGXFWIF_CCB_CMD_TYPE eType;
		PRGXFWIF_UFO_ADDR *pauiIntFenceUFOAddress = NULL;
		PRGXFWIF_UFO_ADDR *pauiIntUpdateUFOAddress = NULL;

		if (TQ_PREP_FLAGS_COMMAND_IS(pui32TQPrepareFlags[i], 3D))
		{
			psServerCommonCtx = psTransferContext->s3DData.psServerCommonContext;
			psClientCCB = FWCommonContextGetClientCCB(psServerCommonCtx);
			pszCommandName = "TQ-3D";
			psCmdHelper = &pas3DCmdHelper[ui323DCmdCount++];
			eType = RGXFWIF_CCB_CMD_TYPE_TQ_3D;
		}
		else if (TQ_PREP_FLAGS_COMMAND_IS(pui32TQPrepareFlags[i], 2D) && \
				(psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK))
		{
			psServerCommonCtx = psTransferContext->s2DData.psServerCommonContext;
			psClientCCB = FWCommonContextGetClientCCB(psServerCommonCtx);
			pszCommandName = "TQ-2D";
			psCmdHelper = &pas2DCmdHelper[ui322DCmdCount++];
			eType = RGXFWIF_CCB_CMD_TYPE_TQ_2D;
		}
		else
		{
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto fail_cmdtype;
		}

		if (i == 0)
		{
			ui32PDumpFlags = ((pui32TQPrepareFlags[i] & TQ_PREP_FLAGS_PDUMPCONTINUOUS) != 0) ? PDUMP_FLAGS_CONTINUOUS : PDUMP_FLAGS_NONE;
			PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags,
					"%s Command Server Submit on FWCtx %08x", pszCommandName, FWCommonContextGetFWAddress(psServerCommonCtx).ui32Addr);
			psTransferContext->ui32PDumpFlags |= ui32PDumpFlags;
		}
		else
		{
			IMG_UINT32 ui32NewPDumpFlags = ((pui32TQPrepareFlags[i] & TQ_PREP_FLAGS_PDUMPCONTINUOUS) != 0) ? PDUMP_FLAGS_CONTINUOUS : PDUMP_FLAGS_NONE;
			if (ui32NewPDumpFlags != ui32PDumpFlags)
			{
				eError = PVRSRV_ERROR_INVALID_PARAMS;
				PVR_DPF((PVR_DBG_ERROR, "%s: Mixing of continuous and non-continuous command in a batch is not permitted", __func__));
				goto fail_pdumpcheck;
			}
		}

		psSyncAddrListFence = &psTransferContext->asSyncAddrListFence[i];
		ui32IntClientFenceCount  = paui32ClientFenceCount[i];
		CHKPT_DBG((PVR_DBG_ERROR, "%s: SyncAddrListPopulate(psTransferContext->sSyncAddrListFence, %d fences)", __func__, ui32IntClientFenceCount));
		eError = SyncAddrListPopulate(psSyncAddrListFence,
										ui32IntClientFenceCount,
										papauiClientFenceUFOSyncPrimBlock[i],
										papaui32ClientFenceSyncOffset[i]);
		if(eError != PVRSRV_OK)
		{
			goto fail_populate_sync_addr_list_fence;
		}
		if (!pauiIntFenceUFOAddress)
		{
			pauiIntFenceUFOAddress = psSyncAddrListFence->pasFWAddrs;
		}

		paui32IntFenceValue      = papaui32ClientFenceValue[i];
		psSyncAddrListUpdate = &psTransferContext->asSyncAddrListUpdate[i];
		ui32IntClientUpdateCount = paui32ClientUpdateCount[i];
		CHKPT_DBG((PVR_DBG_ERROR, "%s: SyncAddrListPopulate(psTransferContext->asSyncAddrListUpdate[], %d updates)", __func__, ui32IntClientUpdateCount));
		eError = SyncAddrListPopulate(psSyncAddrListUpdate,
										ui32IntClientUpdateCount,
										papauiClientUpdateUFOSyncPrimBlock[i],
										papaui32ClientUpdateSyncOffset[i]);
		if(eError != PVRSRV_OK)
		{
			goto fail_populate_sync_addr_list_update;
		}
		if (!pauiIntUpdateUFOAddress)
		{
			pauiIntUpdateUFOAddress = psSyncAddrListUpdate->pasFWAddrs;
		}
		paui32IntUpdateValue     = papaui32ClientUpdateValue[i];

		CHKPT_DBG((PVR_DBG_ERROR, "%s:   (after sync prims) ui32IntClientFenceCount=%d, ui32IntClientUpdateCount=%d", __func__, ui32IntClientFenceCount, ui32IntClientUpdateCount));
		if (ui32SyncPMRCount)
		{
#if defined(SUPPORT_BUFFER_SYNC)
			int err;
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)

			err = pvr_buffer_sync_append_start(psDeviceNode->psBufferSyncContext,
											   ui32SyncPMRCount,
											   ppsSyncPMRs,
											   paui32SyncPMRFlags,
											   ui32IntClientFenceCount,
											   pauiIntFenceUFOAddress,
											   paui32IntFenceValue,
											   ui32IntClientUpdateCount,
											   pauiIntUpdateUFOAddress,
											   paui32IntUpdateValue,
											   &psAppendData);
			if (err)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to append buffer syncs (errno=%d)", __func__, err));
				eError = (err == -ENOMEM) ? PVRSRV_ERROR_OUT_OF_MEMORY : PVRSRV_ERROR_INVALID_PARAMS;
				goto fail_sync_append;
			}

			pvr_buffer_sync_append_checks_get(psAppendData,
											  &ui32IntClientFenceCount,
											  &pauiIntFenceUFOAddress,
											  &paui32IntFenceValue);

			pvr_buffer_sync_append_updates_get(psAppendData,
											   &ui32IntClientUpdateCount,
											   &pauiIntUpdateUFOAddress,
											   &paui32IntUpdateValue);
#else /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
			CHKPT_DBG((PVR_DBG_ERROR, "%s:   Calling pvr_buffer_sync_resolve_and_create_fences", __func__));
			err = pvr_buffer_sync_resolve_and_create_fences(psDeviceNode->psBufferSyncContext,
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
				PVR_DPF((PVR_DBG_ERROR, "%s:   pvr_buffer_sync_resolve_and_create_fences failed (%s)", __func__, PVRSRVGetErrorStringKM(eError)));
				goto fail_resolve_input_fence;
			}

			/* Append buffer sync fences */
			if (ui32BufferFenceSyncCheckpointCount > 0)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d buffer sync checkpoints to TQ Fence (psSyncAddrListFence=<%p>, pauiIntFenceUFOAddress=<%p>)...", __func__, ui32BufferFenceSyncCheckpointCount, (void*)psSyncAddrListFence , (void*)pauiIntFenceUFOAddress));
				SyncAddrListAppendAndDeRefCheckpoints(psSyncAddrListFence,
													  ui32BufferFenceSyncCheckpointCount,
													  apsBufferFenceSyncCheckpoints);
				if (!pauiIntFenceUFOAddress)
				{
					pauiIntFenceUFOAddress = psSyncAddrListFence->pasFWAddrs;
				}
				ui32IntClientFenceCount += ui32BufferFenceSyncCheckpointCount;
			}

			if (psBufferUpdateSyncCheckpoint)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 buffer sync checkpoint<%p> to TQ Update (&psTransferContext->sSyncAddrListUpdate=<%p>, pauiIntUpdateUFOAddress=<%p>)...", __func__, (void*)psBufferUpdateSyncCheckpoint, (void*)&psTransferContext->sSyncAddrListUpdate , (void*)pauiIntUpdateUFOAddress));
				/* Append the update (from output fence) */
				SyncAddrListAppendCheckpoints(psSyncAddrListUpdate,
											  1,
											  &psBufferUpdateSyncCheckpoint);
				if (!pauiIntUpdateUFOAddress)
				{
					pauiIntUpdateUFOAddress = psSyncAddrListUpdate->pasFWAddrs;
				}
				ui32IntClientUpdateCount++;
			}
			CHKPT_DBG((PVR_DBG_ERROR, "%s:   (after buffer_sync) ui32IntClientFenceCount=%d, ui32IntClientUpdateCount=%d", __func__, ui32IntClientFenceCount, ui32IntClientUpdateCount));
#endif /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#else /* defined(SUPPORT_BUFFER_SYNC) */
			PVR_DPF((PVR_DBG_ERROR, "%s: Buffer sync not supported but got %u buffers", __func__, ui32SyncPMRCount));
			PVR_DPF((PVR_DBG_ERROR, "%s:   <--EXIT(%d)", __func__, PVRSRV_ERROR_INVALID_PARAMS));
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
			OSLockRelease(psTransferContext->hLock);
#endif
			return PVRSRV_ERROR_INVALID_PARAMS;
#endif /* defined(SUPPORT_BUFFER_SYNC) */
		}

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
	if (iCheckFence >= 0 || iUpdateTimeline >= 0)
	{
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
		eError =
		  pvr_sync_append_fences(szFenceName,
		                               iCheckFence,
		                               iUpdateTimeline,
		                               ui32IntClientUpdateCount,
		                               pauiIntUpdateUFOAddress,
		                               paui32IntUpdateValue,
		                               ui32IntClientFenceCount,
		                               pauiIntFenceUFOAddress,
		                               paui32IntFenceValue,
		                               &psFDFenceData);
		if (eError != PVRSRV_OK)
		{
			goto fail_syncinit;
		}
		pvr_sync_get_updates(psFDFenceData, &ui32IntClientUpdateCount,
			&pauiIntUpdateUFOAddress, &paui32IntUpdateValue);
		pvr_sync_get_checks(psFDFenceData, &ui32IntClientFenceCount,
			&pauiIntFenceUFOAddress, &paui32IntFenceValue);
#else /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
		CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointResolveFence (iCheckFence=%d), psTransferContext->psDeviceNode->hSyncCheckpointContext=<%p>...", __func__, iCheckFence, (void*)psTransferContext->psDeviceNode->hSyncCheckpointContext));
		/* Resolve the sync checkpoints that make up the input fence */
		eError = SyncCheckpointResolveFence(psTransferContext->psDeviceNode->hSyncCheckpointContext,
											iCheckFence,
											&ui32FenceSyncCheckpointCount,
											&apsFenceSyncCheckpoints,
		                                    &uiCheckFenceUID);
		if (eError != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, returned ERROR (eError=%d)", __func__, eError));
			goto fail_resolve_input_fence;
		}
		CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, fence %d contained %d checkpoints (apsFenceSyncCheckpoints=<%p>)", __func__, iCheckFence, ui32FenceSyncCheckpointCount, (void*)apsFenceSyncCheckpoints));
#if defined(TRANSFER_CHECKPOINT_DEBUG)
		if (ui32FenceSyncCheckpointCount > 0)
		{
			IMG_UINT32 ii;
			for (ii=0; ii<ui32FenceSyncCheckpointCount; ii++)
			{
				PSYNC_CHECKPOINT psNextCheckpoint = *(apsFenceSyncCheckpoints +  ii);
				CHKPT_DBG((PVR_DBG_ERROR, "%s:    apsFenceSyncCheckpoints[%d]=<%p>", __func__, ii, (void*)psNextCheckpoint));
			}
		}
#endif
		/* Create the output fence (if required) */
		if (piUpdateFence)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointCreateFence (iUpdateFence=%d, iUpdateTimeline=%d,  psTranserContext->psDeviceNode->hSyncCheckpointContext=<%p>)", __func__, iUpdateFence, iUpdateTimeline, (void*)psTransferContext->psDeviceNode->hSyncCheckpointContext));
			eError = SyncCheckpointCreateFence(psTransferContext->psDeviceNode,
			                                   szFenceName,
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
				PVR_DPF((PVR_DBG_ERROR, "%s:   SyncCheckpointCreateFence failed (%d)", __func__, eError));
				goto fail_create_output_fence;
			}

			CHKPT_DBG((PVR_DBG_ERROR, "%s: returned from SyncCheckpointCreateFence (iUpdateFence=%d)", __func__, iUpdateFence));

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
				if (psBufferUpdateSyncCheckpoint)
				{
					/* Copy the update values into the new memory, then append our timeline update value */
					OSCachedMemCopy(pui32IntAllocatedUpdateValues, paui32IntUpdateValue, sizeof(*pui32IntAllocatedUpdateValues) * (ui32IntClientUpdateCount-1));
					pui32TimelineUpdateWp = pui32IntAllocatedUpdateValues + (ui32IntClientUpdateCount-1);
				}
				else
				{
					/* Copy the update values into the new memory, then append our timeline update value */
					OSCachedMemCopy(pui32IntAllocatedUpdateValues, paui32IntUpdateValue, sizeof(*pui32IntAllocatedUpdateValues) * ui32IntClientUpdateCount);
					pui32TimelineUpdateWp = pui32IntAllocatedUpdateValues + ui32IntClientUpdateCount;
				}
				CHKPT_DBG((PVR_DBG_ERROR, "%s: Appending the additional update value 0x%x)", __func__, ui32FenceTimelineUpdateValue));
				/* Now set the additional update value */
				*pui32TimelineUpdateWp = ui32FenceTimelineUpdateValue;
#if defined(TRANSFER_CHECKPOINT_DEBUG)
				if (ui32IntClientUpdateCount > 0)
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
				SyncAddrListAppendSyncPrim(psSyncAddrListUpdate,
				                           psFenceTimelineUpdateSync);
				ui32IntClientUpdateCount++;
#if defined(TRANSFER_CHECKPOINT_DEBUG)
				if (ui32IntClientUpdateCount > 0)
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
				CHKPT_DBG((PVR_DBG_ERROR, "%s: set paui32IntUpdateValue<%p> to point to pui32IntAllocatedUpdateValues<%p>", __func__, (void*)paui32IntUpdateValue, (void*)pui32IntAllocatedUpdateValues));
				paui32IntUpdateValue = pui32IntAllocatedUpdateValues;
			}
		}

		if (ui32FenceSyncCheckpointCount)
		{
			/* Append the checks (from input fence) */
			if (ui32FenceSyncCheckpointCount > 0)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to TQ Fence (psSyncAddrListFence=<%p>)...", __func__, ui32FenceSyncCheckpointCount, (void*)psSyncAddrListFence));
				SyncAddrListAppendCheckpoints(psSyncAddrListFence,
											  ui32FenceSyncCheckpointCount,
											  apsFenceSyncCheckpoints);
				if (!pauiIntFenceUFOAddress)
				{
					pauiIntFenceUFOAddress = psSyncAddrListFence->pasFWAddrs;
				}
				ui32IntClientFenceCount += ui32FenceSyncCheckpointCount;
			}
#if defined(TRANSFER_CHECKPOINT_DEBUG)
			if (ui32IntClientFenceCount > 0)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pauiIntFenceUFOAddress;

				for (iii=0; iii<ui32IntClientFenceCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: psSyncAddrListFence->pasFWAddrs[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
					pui32Tmp++;
				}
			}
#endif
		}
		if (psUpdateSyncCheckpoint)
		{
			/* Append the update (from output fence) */
			CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint to TQ Update (psSyncAddrListUpdate=<%p>, pauiIntUpdateUFOAddress=<%p>)...", __func__, (void*)&psTransferContext->asSyncAddrListUpdate , (void*)pauiIntUpdateUFOAddress));
			SyncAddrListAppendCheckpoints(psSyncAddrListUpdate,
										  1,
										  &psUpdateSyncCheckpoint);
			if (!pauiIntUpdateUFOAddress)
			{
				pauiIntUpdateUFOAddress = psSyncAddrListUpdate->pasFWAddrs;
			}
			ui32IntClientUpdateCount++;
#if defined(TRANSFER_CHECKPOINT_DEBUG)
			{
				IMG_UINT32 iii;
				IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pauiIntUpdateUFOAddress;

				for (iii=0; iii<ui32IntClientUpdateCount; iii++)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiIntUpdateUFOAddress[%d](<%p>) = 0x%x", __func__, iii, (void*)pui32Tmp, *pui32Tmp));
					pui32Tmp++;
				}
			}
#endif
		}
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   (after pvr_sync) ui32IntClientFenceCount=%d, ui32IntClientUpdateCount=%d", __func__, ui32IntClientFenceCount, ui32IntClientUpdateCount));
#endif /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
		}
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */

#if (ENABLE_TQ_UFO_DUMP == 1)
		PVR_DPF((PVR_DBG_ERROR, "%s: dumping TQ fence/updates syncs...", __func__));
		{
			IMG_UINT32 ii;
			PRGXFWIF_UFO_ADDR *psTmpIntFenceUFOAddress = pauiIntFenceUFOAddress;
			IMG_UINT32 *pui32TmpIntFenceValue = paui32IntFenceValue;
			PRGXFWIF_UFO_ADDR *psTmpIntUpdateUFOAddress = pauiIntUpdateUFOAddress;
			IMG_UINT32 *pui32TmpIntUpdateValue = paui32IntUpdateValue;

			/* Dump Fence syncs and Update syncs */
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d TQ fence syncs (&psTransferContext->asSyncAddrListFence=<%p>, pauiIntFenceUFOAddress=<%p>):", __func__, ui32IntClientFenceCount, (void*)&psTransferContext->asSyncAddrListFence, (void*)pauiIntFenceUFOAddress));
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
			PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d TQ update syncs (&psTransferContext->asSyncAddrListUpdate=<%p>, pauiIntUpdateUFOAddress=<%p>):", __func__, ui32IntClientUpdateCount, (void*)&psTransferContext->asSyncAddrListUpdate, (void*)pauiIntUpdateUFOAddress));
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
		                                paui32ServerSyncCount[i],
		                                papaui32ServerSyncFlags[i],
		                                SYNC_FLAG_MASK_ALL,
		                                papapsServerSyncs[i],
		                                paui32FWCommandSize[i],
		                                papaui8FWCommand[i],
		                                & pPreAddr,
		                                & pPostAddr,
		                                & pRMWUFOAddr,
		                                eType,
		                                ui32ExtJobRef,
		                                ui32JobId,
		                                ui32PDumpFlags,
		                                NULL,
		                                pszCommandName,
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
	if (ui323DCmdCount)
	{
		eError = RGXCmdHelperAcquireCmdCCB(ui323DCmdCount,
										   &pas3DCmdHelper[0]);
		if (eError != PVRSRV_OK)
		{
			goto fail_3dcmdacquire;
		}
	}

	if (ui322DCmdCount)
	{
		eError = RGXCmdHelperAcquireCmdCCB(ui322DCmdCount,
										   &pas2DCmdHelper[0]);
		if (eError != PVRSRV_OK)
		{
			if (ui323DCmdCount)
			{
				ui323DCmdCount = 0;
				ui322DCmdCount = 0;
			}
			else
			{
				goto fail_2dcmdacquire;
			}
		}
	}

	/*
		We should acquire the kernel CCB(s) space here as the schedule could fail
		and we would have to roll back all the syncs
	*/

	/*
		Only do the command helper release (which takes the server sync
		operations if the acquire succeeded
	*/
	if (ui323DCmdCount)
	{
		ui323DCmdOffset = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psTransferContext->s3DData.psServerCommonContext));
		RGXCmdHelperReleaseCmdCCB(ui323DCmdCount,
								  &pas3DCmdHelper[0],
								  "TQ_3D",
								  FWCommonContextGetFWAddress(psTransferContext->s3DData.psServerCommonContext).ui32Addr);
	}

	if ((ui322DCmdCount) && (psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK))
	{
		ui322DCmdOffset = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psTransferContext->s2DData.psServerCommonContext));
		RGXCmdHelperReleaseCmdCCB(ui322DCmdCount,
								  &pas2DCmdHelper[0],
								  "TQ_2D",
								  FWCommonContextGetFWAddress(psTransferContext->s2DData.psServerCommonContext).ui32Addr);
	}

	/*
		Even if we failed to acquire the client CCB space we might still need
		to kick the HW to process a padding packet to release space for us next
		time round
	*/
	if (ui323DCmdCount)
	{
		RGXFWIF_KCCB_CMD s3DKCCBCmd;
		IMG_UINT32 ui32FWCtx = FWCommonContextGetFWAddress(psTransferContext->s3DData.psServerCommonContext).ui32Addr;

		/* Construct the kernel 3D CCB command. */
		s3DKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
		s3DKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psTransferContext->s3DData.psServerCommonContext);
		s3DKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psTransferContext->s3DData.psServerCommonContext));
		s3DKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;
		s3DKCCBCmd.uCmdData.sCmdKickData.sWorkloadDataFWAddress.ui32Addr = 0;
		s3DKCCBCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = 0;
		HTBLOGK(HTB_SF_MAIN_KICK_3D,
				s3DKCCBCmd.uCmdData.sCmdKickData.psContext,
				ui323DCmdOffset);
		RGX_HWPERF_HOST_ENQ(psTransferContext,
		                    OSGetCurrentClientProcessIDKM(),
		                    ui32FWCtx,
		                    ui32ExtJobRef,
		                    ui32JobId,
		                    RGX_HWPERF_KICK_TYPE_TQ3D,
		                    uiCheckFenceUID,
		                    uiUpdateFenceUID,
		                    NO_DEADLINE,
		                    NO_CYCEST);

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError2 = RGXScheduleCommand(psDeviceNode->pvDevice,
										RGXFWIF_DM_3D,
										&s3DKCCBCmd,
										sizeof(s3DKCCBCmd),
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
				ui32FWCtx, ui32JobId, RGX_HWPERF_KICK_TYPE_TQ3D);
#endif
	}

	if ((ui322DCmdCount) && (psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK))
	{
		RGXFWIF_KCCB_CMD s2DKCCBCmd;
		IMG_UINT32 ui32FWCtx = FWCommonContextGetFWAddress(psTransferContext->s2DData.psServerCommonContext).ui32Addr;

		/* Construct the kernel 2D CCB command. */
		s2DKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
		s2DKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psTransferContext->s2DData.psServerCommonContext);
		s2DKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psTransferContext->s2DData.psServerCommonContext));
		s2DKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;

		HTBLOGK(HTB_SF_MAIN_KICK_2D,
				s2DKCCBCmd.uCmdData.sCmdKickData.psContext,
				ui322DCmdOffset);
		RGX_HWPERF_HOST_ENQ(psTransferContext,
		                    OSGetCurrentClientProcessIDKM(),
		                    ui32FWCtx,
		                    ui32ExtJobRef,
		                    ui32JobId,
		                    RGX_HWPERF_KICK_TYPE_TQ2D,
		                    uiCheckFenceUID,
		                    uiUpdateFenceUID,
		                    NO_DEADLINE,
		                    NO_CYCEST);

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError2 = RGXScheduleCommand(psDeviceNode->pvDevice,
										RGXFWIF_DM_2D,
										&s2DKCCBCmd,
										sizeof(s2DKCCBCmd),
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
				ui32FWCtx, ui32JobId, RGX_HWPERF_KICK_TYPE_TQ2D);
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

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	if (iUpdateTimeline >= 0)
	{
		/* If we get here, this should never fail. Hitting that likely implies
		 * a code error above */
		iUpdateFence = pvr_sync_get_update_fd(psFDFenceData);
		if (iUpdateFence < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to get install update sync fd",
				__func__));
			/* If we fail here, we cannot rollback the syncs as the hw already
			 * has references to resources they may be protecting in the kick
			 * so fallthrough */

			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto fail_free_append_data;
		}
	}
#if defined(NO_HARDWARE)
	pvr_sync_nohw_complete_fences(psFDFenceData);
#endif
	/*
		Free the merged sync memory if required
	*/
	pvr_sync_free_append_fences_data(psFDFenceData);
#else /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#if defined(NO_HARDWARE)
	/* If NO_HARDWARE, signal the output fence's sync checkpoint and sync prim */
	if (psUpdateSyncCheckpoint)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s:   Signalling NOHW sync checkpoint<%p>, ID:%d, FwAddr=0x%x", __func__, (void*)psUpdateSyncCheckpoint, SyncCheckpointGetId(psUpdateSyncCheckpoint), SyncCheckpointGetFirmwareAddr(psUpdateSyncCheckpoint)));
		SyncCheckpointSignalNoHW(psUpdateSyncCheckpoint);
	}
	if (psFenceTimelineUpdateSync)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s:   Updating NOHW sync prim<%p> to %d", __func__, (void*)psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue));
		SyncPrimNoHwUpdate(psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue);
	}
	SyncCheckpointNoHWUpdateTimelines(NULL);
#endif /* defined (NO_HARDWARE) */
#endif /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */

#if defined(SUPPORT_BUFFER_SYNC)
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	if (psAppendData)
	{
		pvr_buffer_sync_append_finish(psAppendData);
	}
#else /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
	if (psBufferSyncData)
	{
		pvr_buffer_sync_kick_succeeded(psBufferSyncData);
	}
	if (apsBufferFenceSyncCheckpoints)
	{
		kfree(apsBufferFenceSyncCheckpoints);
	}
#endif /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#endif /* defined(SUPPORT_BUFFER_SYNC) */

	if (piUpdateFence)
	{
		*piUpdateFence = iUpdateFence;
	}
#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	if (pvUpdateFenceFinaliseData && (iUpdateFence != PVRSRV_FENCE_INVALID))
	{
		SyncCheckpointFinaliseFence(iUpdateFence, pvUpdateFenceFinaliseData);
	}
#endif

	OSFreeMem(pas2DCmdHelper);
	OSFreeMem(pas3DCmdHelper);

#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
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
#endif /* defined(PVRSRV_USE_SYNC_CHECKPOINTS) */

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
#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	SyncAddrListRollbackCheckpoints(psTransferContext->psDeviceNode, psSyncAddrListFence);
	SyncAddrListRollbackCheckpoints(psTransferContext->psDeviceNode, psSyncAddrListUpdate);
fail_alloc_update_values_mem:
#endif
#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	if(iUpdateFence != PVRSRV_FENCE_INVALID)
	{
		SyncCheckpointRollbackFenceData(iUpdateFence, pvUpdateFenceFinaliseData);
	}
fail_create_output_fence:
	/* Drop the references taken on the sync checkpoints in the
	 * resolved input fence */
	SyncAddrListDeRefCheckpoints(ui32FenceSyncCheckpointCount,
								 apsFenceSyncCheckpoints);
fail_resolve_input_fence:
#endif /* defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */
fail_pdumpcheck:
fail_cmdtype:
#if defined(SUPPORT_NATIVE_FENCE_SYNC) && !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
fail_syncinit:
	/* Relocated cleanup here as the loop could fail after the first iteration
	 * at the above goto tags at which point the psFDCheckData memory would
	 * have been allocated.
	 */
	pvr_sync_rollback_append_fences(psFDFenceData);
fail_free_append_data:
	pvr_sync_free_append_fences_data(psFDFenceData);
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) && !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#if defined(SUPPORT_BUFFER_SYNC)
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	pvr_buffer_sync_append_abort(psAppendData);
fail_sync_append:
#else /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
	if (psBufferSyncData)
	{
		pvr_buffer_sync_kick_failed(psBufferSyncData);
	}
	if (apsBufferFenceSyncCheckpoints)
	{
		kfree(apsBufferFenceSyncCheckpoints);
	}
#endif /* !defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#endif /* defined(SUPPORT_BUFFER_SYNC) */
fail_populate_sync_addr_list_update:
fail_populate_sync_addr_list_fence:
	PVR_ASSERT(eError != PVRSRV_OK);
	OSFreeMem(pas2DCmdHelper);
fail_alloc2dhelper:
	OSFreeMem(pas3DCmdHelper);
fail_alloc3dhelper:
#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
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
#endif /* defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psTransferContext->hLock);
#endif
	return eError;
}


PVRSRV_ERROR PVRSRVRGXSetTransferContextPriorityKM(CONNECTION_DATA *psConnection,
                                                   PVRSRV_DEVICE_NODE * psDevNode,
												   RGX_SERVER_TQ_CONTEXT *psTransferContext,
												   IMG_UINT32 ui32Priority)
{
	PVRSRV_ERROR eError;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;

	PVR_UNREFERENCED_PARAMETER(psDevNode);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psTransferContext->hLock);
#endif

	if ((psTransferContext->s2DData.ui32Priority != ui32Priority)  && \
			(psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK))
	{
		eError = ContextSetPriority(psTransferContext->s2DData.psServerCommonContext,
									psConnection,
									psTransferContext->psDeviceNode->pvDevice,
									ui32Priority,
									RGXFWIF_DM_2D);
		if (eError != PVRSRV_OK)
		{
			if(eError != PVRSRV_ERROR_RETRY)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set the priority of the 2D part of the transfercontext (%s)", __func__, PVRSRVGetErrorStringKM(eError)));
			}
			goto fail_2dcontext;
		}
		psTransferContext->s2DData.ui32Priority = ui32Priority;
	}

	if (psTransferContext->s3DData.ui32Priority != ui32Priority)
	{
		eError = ContextSetPriority(psTransferContext->s3DData.psServerCommonContext,
									psConnection,
									psTransferContext->psDeviceNode->pvDevice,
									ui32Priority,
									RGXFWIF_DM_3D);
		if (eError != PVRSRV_OK)
		{
			if(eError != PVRSRV_ERROR_RETRY)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set the priority of the 3D part of the transfercontext (%s)", __func__, PVRSRVGetErrorStringKM(eError)));
			}
			goto fail_3dcontext;
		}
		psTransferContext->s3DData.ui32Priority = ui32Priority;
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psTransferContext->hLock);
#endif
	return PVRSRV_OK;

fail_3dcontext:

fail_2dcontext:
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psTransferContext->hLock);
#endif
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

void CheckForStalledTransferCtxt(PVRSRV_RGXDEV_INFO *psDevInfo,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	DLLIST_NODE *psNode, *psNext;

	OSWRLockAcquireRead(psDevInfo->hTransferCtxListLock);

	dllist_foreach_node(&psDevInfo->sTransferCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_TQ_CONTEXT *psCurrentServerTransferCtx =
			IMG_CONTAINER_OF(psNode, RGX_SERVER_TQ_CONTEXT, sListNode);

		if ((psCurrentServerTransferCtx->ui32Flags & RGX_SERVER_TQ_CONTEXT_FLAGS_2D) && \
				(psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK))
		{
			DumpStalledFWCommonContext(psCurrentServerTransferCtx->s2DData.psServerCommonContext,
									   pfnDumpDebugPrintf, pvDumpDebugFile);
		}

		if (psCurrentServerTransferCtx->ui32Flags & RGX_SERVER_TQ_CONTEXT_FLAGS_3D)
		{
			DumpStalledFWCommonContext(psCurrentServerTransferCtx->s3DData.psServerCommonContext,
									   pfnDumpDebugPrintf, pvDumpDebugFile);
		}
	}

	OSWRLockReleaseRead(psDevInfo->hTransferCtxListLock);
}

IMG_UINT32 CheckForStalledClientTransferCtxt(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	DLLIST_NODE *psNode, *psNext;
	IMG_UINT32 ui32ContextBitMask = 0;

	OSWRLockAcquireRead(psDevInfo->hTransferCtxListLock);

	dllist_foreach_node(&psDevInfo->sTransferCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_TQ_CONTEXT *psCurrentServerTransferCtx =
			IMG_CONTAINER_OF(psNode, RGX_SERVER_TQ_CONTEXT, sListNode);

		if ((psCurrentServerTransferCtx->ui32Flags & RGX_SERVER_TQ_CONTEXT_FLAGS_2D) && \
				(NULL != psCurrentServerTransferCtx->s2DData.psServerCommonContext) && \
				(psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_TLA_BIT_MASK))
		{
			if (CheckStalledClientCommonContext(psCurrentServerTransferCtx->s2DData.psServerCommonContext, RGX_KICK_TYPE_DM_TQ2D) == PVRSRV_ERROR_CCCB_STALLED)
			{
				ui32ContextBitMask |= RGX_KICK_TYPE_DM_TQ2D;
			}
		}

		if ((psCurrentServerTransferCtx->ui32Flags & RGX_SERVER_TQ_CONTEXT_FLAGS_3D) && (NULL != psCurrentServerTransferCtx->s3DData.psServerCommonContext))
		{
			if ((CheckStalledClientCommonContext(psCurrentServerTransferCtx->s3DData.psServerCommonContext, RGX_KICK_TYPE_DM_TQ3D) == PVRSRV_ERROR_CCCB_STALLED))
			{
				ui32ContextBitMask |= RGX_KICK_TYPE_DM_TQ3D;
			}
		}
	}

	OSWRLockReleaseRead(psDevInfo->hTransferCtxListLock);
	return ui32ContextBitMask;
}

/**************************************************************************//**
 End of file (rgxtransfer.c)
******************************************************************************/
