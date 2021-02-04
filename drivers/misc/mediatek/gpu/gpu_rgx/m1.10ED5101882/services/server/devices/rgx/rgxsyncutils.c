/**********************************
 * RGX Sync helper functions
 **********************************/
#include "rgxsyncutils.h"

#include "sync_server.h"
#include "sync_internal.h"
#include "sync.h"

#if defined(SUPPORT_BUFFER_SYNC)
#include "pvr_buffer_sync.h"
#endif

#include "sync_checkpoint.h"
#include "sync_checkpoint_internal.h"

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
#include "pvrsrv_sync_server.h"
#endif

#if defined(SUPPORT_NATIVE_FENCE_SYNC)
#include "pvr_sync.h"
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) */

//#define TA3D_CHECKPOINT_DEBUG

#if defined(TA3D_CHECKPOINT_DEBUG)
#define CHKPT_DBG(X) PVR_DPF(X)
static
void _DebugSyncValues(IMG_UINT32 *pui32UpdateValues,
					  IMG_UINT32 ui32Count)
{
	IMG_UINT32 iii;
	IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32UpdateValues;

	for (iii=0; iii<ui32Count; iii++)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", __FUNCTION__, iii, (void*)pui32Tmp, *pui32Tmp));
		pui32Tmp++;
	}
}
#else
#define CHKPT_DBG(X)
#endif


PVRSRV_ERROR RGXSyncAppendTimelineUpdate(IMG_UINT32 ui32FenceTimelineUpdateValue,
										 SYNC_ADDR_LIST	*psSyncList,
										 SYNC_ADDR_LIST	*psPRSyncList,
										 PVRSRV_CLIENT_SYNC_PRIM *psFenceTimelineUpdateSync,
										 RGX_SYNC_DATA *psSyncData,
										 IMG_BOOL bKick3D)
{
	IMG_UINT32 *pui32TimelineUpdateWOff = NULL;
	IMG_UINT32 *pui32IntAllocatedUpdateValues = NULL;

	IMG_UINT32	ui32ClientUpdateValueCount = psSyncData->ui32ClientUpdateValueCount;
	IMG_UINT32	ui32ClientPRUpdateValueCount = psSyncData->ui32ClientPRUpdateValueCount;

	size_t	uiUpdateSize = sizeof(*pui32IntAllocatedUpdateValues) * (ui32ClientUpdateValueCount+1);

	if (!bKick3D)
	{
		uiUpdateSize = sizeof(*pui32IntAllocatedUpdateValues) * (ui32ClientPRUpdateValueCount+1);
	}

	CHKPT_DBG((PVR_DBG_ERROR, "%s: About to allocate memory to hold updates in pui32IntAllocatedUpdateValues(<%p>)", __FUNCTION__, \
		(void*)pui32IntAllocatedUpdateValues));

	/* Allocate memory to hold the list of update values (including our timeline update) */
	pui32IntAllocatedUpdateValues = OSAllocMem(uiUpdateSize);
	if (!pui32IntAllocatedUpdateValues)
	{
		/* Failed to allocate memory */
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	OSCachedMemSet(pui32IntAllocatedUpdateValues, 0xcc, uiUpdateSize);

	if (bKick3D)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: Copying %d 3D update values into pui32IntAllocatedUpdateValues(<%p>)", __FUNCTION__, \
			ui32ClientUpdateValueCount, (void*)pui32IntAllocatedUpdateValues));
		/* Copy the update values into the new memory, then append our timeline update value */
		OSCachedMemCopy(pui32IntAllocatedUpdateValues, psSyncData->paui32ClientUpdateValue, ui32ClientUpdateValueCount * sizeof(*psSyncData->paui32ClientUpdateValue));

#if defined(TA3D_CHECKPOINT_DEBUG)
		_DebugSyncValues(pui32IntAllocatedUpdateValues, ui32ClientUpdateValueCount);
#endif
	}

	/* Now set the additional update value and append the timeline sync prim addr to either the
	 * render context 3D (or TA) update list
	 */
	CHKPT_DBG((PVR_DBG_ERROR, "%s: Appending the additional update value (0x%x) to psRenderContext->sSyncAddrList%sUpdate...", __FUNCTION__, \
		ui32FenceTimelineUpdateValue, bKick3D ? "3D" : "TA"));

	/* Append the TA/3D update */
	{
		pui32TimelineUpdateWOff = pui32IntAllocatedUpdateValues + ui32ClientUpdateValueCount;
		*pui32TimelineUpdateWOff = ui32FenceTimelineUpdateValue;
		psSyncData->ui32ClientUpdateValueCount++;
		psSyncData->ui32ClientUpdateCount++;
		SyncAddrListAppendSyncPrim(psSyncList, psFenceTimelineUpdateSync);

		if (!psSyncData->pauiClientUpdateUFOAddress)
		{
			psSyncData->pauiClientUpdateUFOAddress = psSyncList->pasFWAddrs;
		}
		/* Update paui32ClientUpdateValue to point to our new list of update values */
		psSyncData->paui32ClientUpdateValue = pui32IntAllocatedUpdateValues;
	}

	if (!bKick3D)
	{
		/* Use the sSyncAddrList3DUpdate for PR (as it doesn't have one of its own) */
		pui32TimelineUpdateWOff = pui32IntAllocatedUpdateValues;
		*pui32TimelineUpdateWOff = ui32FenceTimelineUpdateValue;
		psSyncData->ui32ClientPRUpdateValueCount++;
		psSyncData->ui32ClientPRUpdateCount++;
		SyncAddrListAppendSyncPrim(psPRSyncList, psFenceTimelineUpdateSync);

		if (!psSyncData->pauiClientPRUpdateUFOAddress)
		{
			psSyncData->pauiClientPRUpdateUFOAddress = psPRSyncList->pasFWAddrs;
		}
		/* Update paui32ClientPRUpdateValue to point to our new list of update values */
		psSyncData->paui32ClientPRUpdateValue = pui32IntAllocatedUpdateValues;
	}
#if defined(TA3D_CHECKPOINT_DEBUG)
	if (bKick3D)
	{
		_DebugSyncValues(pui32IntAllocatedUpdateValues, psSyncData->ui32ClientUpdateValueCount);
	}
#endif

	return PVRSRV_OK;
}
