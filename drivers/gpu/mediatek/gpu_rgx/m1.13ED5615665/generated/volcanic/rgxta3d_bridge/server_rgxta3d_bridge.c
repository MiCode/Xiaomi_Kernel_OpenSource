/*******************************************************************************
@File
@Title          Server bridge for rgxta3d
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxta3d
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
*******************************************************************************/

#include <linux/uaccess.h>

#include "img_defs.h"

#include "rgxta3d.h"

#include "common_rgxta3d_bridge.h"

#include "allocmem.h"
#include "pvr_debug.h"
#include "connection_server.h"
#include "pvr_bridge.h"
#if defined(SUPPORT_RGX)
#include "rgx_bridge.h"
#endif
#include "srvcore.h"
#include "handle.h"

#include <linux/slab.h>

/* ***************************************************************************
 * Server-side bridge entry points
 */

static IMG_INT
PVRSRVBridgeRGXCreateHWRTDataSet(IMG_UINT32 ui32DispatchTableEntry,
				 PVRSRV_BRIDGE_IN_RGXCREATEHWRTDATASET *
				 psRGXCreateHWRTDataSetIN,
				 PVRSRV_BRIDGE_OUT_RGXCREATEHWRTDATASET *
				 psRGXCreateHWRTDataSetOUT,
				 CONNECTION_DATA * psConnection)
{
	RGX_FREELIST **psapsFreeListsInt = NULL;
	IMG_HANDLE *hapsFreeListsInt2 = NULL;
	RGX_KM_HW_RT_DATASET *psKmHwRTDataSetInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (RGXFW_MAX_FREELISTS * sizeof(RGX_FREELIST *)) +
	    (RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE)) + 0;

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXCreateHWRTDataSetIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psRGXCreateHWRTDataSetIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXCreateHWRTDataSetOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXCreateHWRTDataSet_exit;
			}
		}
	}

	{
		psapsFreeListsInt =
		    (RGX_FREELIST **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				       ui32NextOffset);
		ui32NextOffset += RGXFW_MAX_FREELISTS * sizeof(RGX_FREELIST *);
		hapsFreeListsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset += RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hapsFreeListsInt2,
		     (const void __user *)psRGXCreateHWRTDataSetIN->
		     phapsFreeLists,
		     RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXCreateHWRTDataSetOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXCreateHWRTDataSet_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	{
		IMG_UINT32 i;

		for (i = 0; i < RGXFW_MAX_FREELISTS; i++)
		{
			/* Look up the address from the handle */
			psRGXCreateHWRTDataSetOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psapsFreeListsInt[i],
						       hapsFreeListsInt2[i],
						       PVRSRV_HANDLE_TYPE_RGX_FREELIST,
						       IMG_TRUE);
			if (unlikely
			    (psRGXCreateHWRTDataSetOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXCreateHWRTDataSet_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXCreateHWRTDataSetOUT->eError =
	    RGXCreateHWRTDataSet(psConnection, OSGetDevNode(psConnection),
				 psRGXCreateHWRTDataSetIN->sVHeapTableDevVAddr,
				 psRGXCreateHWRTDataSetIN->sPMDataAddr,
				 psRGXCreateHWRTDataSetIN->sPMSecureDataAddr,
				 psapsFreeListsInt,
				 psRGXCreateHWRTDataSetIN->ui32PPPScreen,
				 psRGXCreateHWRTDataSetIN->
				 ui64PPPMultiSampleCtl,
				 psRGXCreateHWRTDataSetIN->ui32TPCStride,
				 psRGXCreateHWRTDataSetIN->sTailPtrsDevVAddr,
				 psRGXCreateHWRTDataSetIN->ui32TPCSize,
				 psRGXCreateHWRTDataSetIN->ui32TEScreen,
				 psRGXCreateHWRTDataSetIN->ui32TEAA,
				 psRGXCreateHWRTDataSetIN->ui32TEMTILE1,
				 psRGXCreateHWRTDataSetIN->ui32TEMTILE2,
				 psRGXCreateHWRTDataSetIN->ui32RgnStride,
				 psRGXCreateHWRTDataSetIN->ui32ISPMergeLowerX,
				 psRGXCreateHWRTDataSetIN->ui32ISPMergeLowerY,
				 psRGXCreateHWRTDataSetIN->ui32ISPMergeUpperX,
				 psRGXCreateHWRTDataSetIN->ui32ISPMergeUpperY,
				 psRGXCreateHWRTDataSetIN->ui32ISPMergeScaleX,
				 psRGXCreateHWRTDataSetIN->ui32ISPMergeScaleY,
				 psRGXCreateHWRTDataSetIN->ui16MaxRTs,
				 &psKmHwRTDataSetInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXCreateHWRTDataSetOUT->eError != PVRSRV_OK))
	{
		goto RGXCreateHWRTDataSet_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXCreateHWRTDataSetOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXCreateHWRTDataSetOUT->
				      hKmHwRTDataSet,
				      (void *)psKmHwRTDataSetInt,
				      PVRSRV_HANDLE_TYPE_RGX_KM_HW_RT_DATASET,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      RGXDestroyHWRTDataSet);
	if (unlikely(psRGXCreateHWRTDataSetOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateHWRTDataSet_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXCreateHWRTDataSet_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	if (hapsFreeListsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < RGXFW_MAX_FREELISTS; i++)
		{

			/* Unreference the previously looked up handle */
			if (hapsFreeListsInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hapsFreeListsInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_RGX_FREELIST);
			}
		}
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psRGXCreateHWRTDataSetOUT->eError != PVRSRV_OK)
	{
		if (psKmHwRTDataSetInt)
		{
			RGXDestroyHWRTDataSet(psKmHwRTDataSetInt);
		}
	}

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyHWRTDataSet(IMG_UINT32 ui32DispatchTableEntry,
				  PVRSRV_BRIDGE_IN_RGXDESTROYHWRTDATASET *
				  psRGXDestroyHWRTDataSetIN,
				  PVRSRV_BRIDGE_OUT_RGXDESTROYHWRTDATASET *
				  psRGXDestroyHWRTDataSetOUT,
				  CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXDestroyHWRTDataSetOUT->eError =
	    PVRSRVReleaseHandleStagedUnlock(psConnection->psHandleBase,
					    (IMG_HANDLE)
					    psRGXDestroyHWRTDataSetIN->
					    hKmHwRTDataSet,
					    PVRSRV_HANDLE_TYPE_RGX_KM_HW_RT_DATASET);
	if (unlikely
	    ((psRGXDestroyHWRTDataSetOUT->eError != PVRSRV_OK)
	     && (psRGXDestroyHWRTDataSetOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeRGXDestroyHWRTDataSet: %s",
			 PVRSRVGetErrorString(psRGXDestroyHWRTDataSetOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXDestroyHWRTDataSet_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXDestroyHWRTDataSet_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCreateZSBuffer(IMG_UINT32 ui32DispatchTableEntry,
			      PVRSRV_BRIDGE_IN_RGXCREATEZSBUFFER *
			      psRGXCreateZSBufferIN,
			      PVRSRV_BRIDGE_OUT_RGXCREATEZSBUFFER *
			      psRGXCreateZSBufferOUT,
			      CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hReservation = psRGXCreateZSBufferIN->hReservation;
	DEVMEMINT_RESERVATION *psReservationInt = NULL;
	IMG_HANDLE hPMR = psRGXCreateZSBufferIN->hPMR;
	PMR *psPMRInt = NULL;
	RGX_ZSBUFFER_DATA *pssZSBufferKMInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXCreateZSBufferOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psReservationInt,
				       hReservation,
				       PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
				       IMG_TRUE);
	if (unlikely(psRGXCreateZSBufferOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateZSBuffer_exit;
	}

	/* Look up the address from the handle */
	psRGXCreateZSBufferOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPMRInt,
				       hPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psRGXCreateZSBufferOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateZSBuffer_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXCreateZSBufferOUT->eError =
	    RGXCreateZSBufferKM(psConnection, OSGetDevNode(psConnection),
				psReservationInt,
				psPMRInt,
				psRGXCreateZSBufferIN->uiMapFlags,
				&pssZSBufferKMInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXCreateZSBufferOUT->eError != PVRSRV_OK))
	{
		goto RGXCreateZSBuffer_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXCreateZSBufferOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXCreateZSBufferOUT->hsZSBufferKM,
				      (void *)pssZSBufferKMInt,
				      PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      RGXDestroyZSBufferKM);
	if (unlikely(psRGXCreateZSBufferOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateZSBuffer_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXCreateZSBuffer_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psReservationInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hReservation,
					    PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
	}

	/* Unreference the previously looked up handle */
	if (psPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
	{
		if (pssZSBufferKMInt)
		{
			RGXDestroyZSBufferKM(pssZSBufferKMInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyZSBuffer(IMG_UINT32 ui32DispatchTableEntry,
			       PVRSRV_BRIDGE_IN_RGXDESTROYZSBUFFER *
			       psRGXDestroyZSBufferIN,
			       PVRSRV_BRIDGE_OUT_RGXDESTROYZSBUFFER *
			       psRGXDestroyZSBufferOUT,
			       CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXDestroyZSBufferOUT->eError =
	    PVRSRVReleaseHandleStagedUnlock(psConnection->psHandleBase,
					    (IMG_HANDLE)
					    psRGXDestroyZSBufferIN->
					    hsZSBufferMemDesc,
					    PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
	if (unlikely
	    ((psRGXDestroyZSBufferOUT->eError != PVRSRV_OK)
	     && (psRGXDestroyZSBufferOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeRGXDestroyZSBuffer: %s",
			 PVRSRVGetErrorString(psRGXDestroyZSBufferOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXDestroyZSBuffer_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXDestroyZSBuffer_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXPopulateZSBuffer(IMG_UINT32 ui32DispatchTableEntry,
				PVRSRV_BRIDGE_IN_RGXPOPULATEZSBUFFER *
				psRGXPopulateZSBufferIN,
				PVRSRV_BRIDGE_OUT_RGXPOPULATEZSBUFFER *
				psRGXPopulateZSBufferOUT,
				CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hsZSBufferKM = psRGXPopulateZSBufferIN->hsZSBufferKM;
	RGX_ZSBUFFER_DATA *pssZSBufferKMInt = NULL;
	RGX_POPULATION *pssPopulationInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXPopulateZSBufferOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&pssZSBufferKMInt,
				       hsZSBufferKM,
				       PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
				       IMG_TRUE);
	if (unlikely(psRGXPopulateZSBufferOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXPopulateZSBuffer_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXPopulateZSBufferOUT->eError =
	    RGXPopulateZSBufferKM(pssZSBufferKMInt, &pssPopulationInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXPopulateZSBufferOUT->eError != PVRSRV_OK))
	{
		goto RGXPopulateZSBuffer_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXPopulateZSBufferOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXPopulateZSBufferOUT->hsPopulation,
				      (void *)pssPopulationInt,
				      PVRSRV_HANDLE_TYPE_RGX_POPULATION,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      RGXUnpopulateZSBufferKM);
	if (unlikely(psRGXPopulateZSBufferOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXPopulateZSBuffer_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXPopulateZSBuffer_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (pssZSBufferKMInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hsZSBufferKM,
					    PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
	{
		if (pssPopulationInt)
		{
			RGXUnpopulateZSBufferKM(pssPopulationInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXUnpopulateZSBuffer(IMG_UINT32 ui32DispatchTableEntry,
				  PVRSRV_BRIDGE_IN_RGXUNPOPULATEZSBUFFER *
				  psRGXUnpopulateZSBufferIN,
				  PVRSRV_BRIDGE_OUT_RGXUNPOPULATEZSBUFFER *
				  psRGXUnpopulateZSBufferOUT,
				  CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXUnpopulateZSBufferOUT->eError =
	    PVRSRVReleaseHandleStagedUnlock(psConnection->psHandleBase,
					    (IMG_HANDLE)
					    psRGXUnpopulateZSBufferIN->
					    hsPopulation,
					    PVRSRV_HANDLE_TYPE_RGX_POPULATION);
	if (unlikely
	    ((psRGXUnpopulateZSBufferOUT->eError != PVRSRV_OK)
	     && (psRGXUnpopulateZSBufferOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeRGXUnpopulateZSBuffer: %s",
			 PVRSRVGetErrorString(psRGXUnpopulateZSBufferOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXUnpopulateZSBuffer_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXUnpopulateZSBuffer_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCreateFreeList(IMG_UINT32 ui32DispatchTableEntry,
			      PVRSRV_BRIDGE_IN_RGXCREATEFREELIST *
			      psRGXCreateFreeListIN,
			      PVRSRV_BRIDGE_OUT_RGXCREATEFREELIST *
			      psRGXCreateFreeListOUT,
			      CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hsGlobalFreeList = psRGXCreateFreeListIN->hsGlobalFreeList;
	RGX_FREELIST *pssGlobalFreeListInt = NULL;
	IMG_HANDLE hsFreeListPMR = psRGXCreateFreeListIN->hsFreeListPMR;
	PMR *pssFreeListPMRInt = NULL;
	IMG_HANDLE hsFreeListStatePMR =
	    psRGXCreateFreeListIN->hsFreeListStatePMR;
	PMR *pssFreeListStatePMRInt = NULL;
	RGX_FREELIST *psCleanupCookieInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	if (psRGXCreateFreeListIN->hsGlobalFreeList)
	{
		/* Look up the address from the handle */
		psRGXCreateFreeListOUT->eError =
		    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
					       (void **)&pssGlobalFreeListInt,
					       hsGlobalFreeList,
					       PVRSRV_HANDLE_TYPE_RGX_FREELIST,
					       IMG_TRUE);
		if (unlikely(psRGXCreateFreeListOUT->eError != PVRSRV_OK))
		{
			UnlockHandle(psConnection->psHandleBase);
			goto RGXCreateFreeList_exit;
		}
	}

	/* Look up the address from the handle */
	psRGXCreateFreeListOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&pssFreeListPMRInt,
				       hsFreeListPMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psRGXCreateFreeListOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateFreeList_exit;
	}

	/* Look up the address from the handle */
	psRGXCreateFreeListOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&pssFreeListStatePMRInt,
				       hsFreeListStatePMR,
				       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
				       IMG_TRUE);
	if (unlikely(psRGXCreateFreeListOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateFreeList_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXCreateFreeListOUT->eError =
	    RGXCreateFreeList(psConnection, OSGetDevNode(psConnection),
			      psRGXCreateFreeListIN->ui32MaxFLPages,
			      psRGXCreateFreeListIN->ui32InitFLPages,
			      psRGXCreateFreeListIN->ui32GrowFLPages,
			      psRGXCreateFreeListIN->ui32GrowParamThreshold,
			      pssGlobalFreeListInt,
			      psRGXCreateFreeListIN->bbFreeListCheck,
			      psRGXCreateFreeListIN->spsFreeListBaseDevVAddr,
			      psRGXCreateFreeListIN->spsFreeListStateDevVAddr,
			      pssFreeListPMRInt,
			      psRGXCreateFreeListIN->uiPMROffset,
			      pssFreeListStatePMRInt,
			      psRGXCreateFreeListIN->uiPMRStateOffset,
			      &psCleanupCookieInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXCreateFreeListOUT->eError != PVRSRV_OK))
	{
		goto RGXCreateFreeList_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXCreateFreeListOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXCreateFreeListOUT->hCleanupCookie,
				      (void *)psCleanupCookieInt,
				      PVRSRV_HANDLE_TYPE_RGX_FREELIST,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      RGXDestroyFreeList);
	if (unlikely(psRGXCreateFreeListOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateFreeList_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXCreateFreeList_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	if (psRGXCreateFreeListIN->hsGlobalFreeList)
	{

		/* Unreference the previously looked up handle */
		if (pssGlobalFreeListInt)
		{
			PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
						    hsGlobalFreeList,
						    PVRSRV_HANDLE_TYPE_RGX_FREELIST);
		}
	}

	/* Unreference the previously looked up handle */
	if (pssFreeListPMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hsFreeListPMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}

	/* Unreference the previously looked up handle */
	if (pssFreeListStatePMRInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hsFreeListStatePMR,
					    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psRGXCreateFreeListOUT->eError != PVRSRV_OK)
	{
		if (psCleanupCookieInt)
		{
			RGXDestroyFreeList(psCleanupCookieInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyFreeList(IMG_UINT32 ui32DispatchTableEntry,
			       PVRSRV_BRIDGE_IN_RGXDESTROYFREELIST *
			       psRGXDestroyFreeListIN,
			       PVRSRV_BRIDGE_OUT_RGXDESTROYFREELIST *
			       psRGXDestroyFreeListOUT,
			       CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXDestroyFreeListOUT->eError =
	    PVRSRVReleaseHandleStagedUnlock(psConnection->psHandleBase,
					    (IMG_HANDLE)
					    psRGXDestroyFreeListIN->
					    hCleanupCookie,
					    PVRSRV_HANDLE_TYPE_RGX_FREELIST);
	if (unlikely
	    ((psRGXDestroyFreeListOUT->eError != PVRSRV_OK)
	     && (psRGXDestroyFreeListOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeRGXDestroyFreeList: %s",
			 PVRSRVGetErrorString(psRGXDestroyFreeListOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXDestroyFreeList_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXDestroyFreeList_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCreateRenderContext(IMG_UINT32 ui32DispatchTableEntry,
				   PVRSRV_BRIDGE_IN_RGXCREATERENDERCONTEXT *
				   psRGXCreateRenderContextIN,
				   PVRSRV_BRIDGE_OUT_RGXCREATERENDERCONTEXT *
				   psRGXCreateRenderContextOUT,
				   CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hPrivData = psRGXCreateRenderContextIN->hPrivData;
	IMG_HANDLE hPrivDataInt = NULL;
	IMG_BYTE *psStaticRenderContextStateInt = NULL;
	RGX_SERVER_RENDER_CONTEXT *psRenderContextInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXCreateRenderContextIN->ui32StaticRenderContextStateSize *
	     sizeof(IMG_BYTE)) + 0;

	if (unlikely
	    (psRGXCreateRenderContextIN->ui32StaticRenderContextStateSize >
	     RGXFWIF_STATIC_RENDERCONTEXT_SIZE))
	{
		psRGXCreateRenderContextOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXCreateRenderContext_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXCreateRenderContextIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psRGXCreateRenderContextIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXCreateRenderContextOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXCreateRenderContext_exit;
			}
		}
	}

	if (psRGXCreateRenderContextIN->ui32StaticRenderContextStateSize != 0)
	{
		psStaticRenderContextStateInt =
		    (IMG_BYTE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psRGXCreateRenderContextIN->
		    ui32StaticRenderContextStateSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXCreateRenderContextIN->ui32StaticRenderContextStateSize *
	    sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, psStaticRenderContextStateInt,
		     (const void __user *)psRGXCreateRenderContextIN->
		     psStaticRenderContextState,
		     psRGXCreateRenderContextIN->
		     ui32StaticRenderContextStateSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXCreateRenderContextOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXCreateRenderContext_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXCreateRenderContextOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&hPrivDataInt,
				       hPrivData,
				       PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
				       IMG_TRUE);
	if (unlikely(psRGXCreateRenderContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateRenderContext_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXCreateRenderContextOUT->eError =
	    PVRSRVRGXCreateRenderContextKM(psConnection,
					   OSGetDevNode(psConnection),
					   psRGXCreateRenderContextIN->
					   ui32Priority, hPrivDataInt,
					   psRGXCreateRenderContextIN->
					   ui32StaticRenderContextStateSize,
					   psStaticRenderContextStateInt,
					   psRGXCreateRenderContextIN->
					   ui32PackedCCBSizeU8888,
					   psRGXCreateRenderContextIN->
					   ui32ContextFlags,
					   psRGXCreateRenderContextIN->
					   ui64RobustnessAddress,
					   psRGXCreateRenderContextIN->
					   ui32MaxTADeadlineMS,
					   psRGXCreateRenderContextIN->
					   ui32Max3DDeadlineMS,
					   &psRenderContextInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXCreateRenderContextOUT->eError != PVRSRV_OK))
	{
		goto RGXCreateRenderContext_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXCreateRenderContextOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXCreateRenderContextOUT->
				      hRenderContext,
				      (void *)psRenderContextInt,
				      PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      PVRSRVRGXDestroyRenderContextKM);
	if (unlikely(psRGXCreateRenderContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateRenderContext_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXCreateRenderContext_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (hPrivDataInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPrivData,
					    PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
	{
		if (psRenderContextInt)
		{
			PVRSRVRGXDestroyRenderContextKM(psRenderContextInt);
		}
	}

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyRenderContext(IMG_UINT32 ui32DispatchTableEntry,
				    PVRSRV_BRIDGE_IN_RGXDESTROYRENDERCONTEXT *
				    psRGXDestroyRenderContextIN,
				    PVRSRV_BRIDGE_OUT_RGXDESTROYRENDERCONTEXT *
				    psRGXDestroyRenderContextOUT,
				    CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXDestroyRenderContextOUT->eError =
	    PVRSRVReleaseHandleStagedUnlock(psConnection->psHandleBase,
					    (IMG_HANDLE)
					    psRGXDestroyRenderContextIN->
					    hCleanupCookie,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
	if (unlikely
	    ((psRGXDestroyRenderContextOUT->eError != PVRSRV_OK)
	     && (psRGXDestroyRenderContextOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeRGXDestroyRenderContext: %s",
			 PVRSRVGetErrorString(psRGXDestroyRenderContextOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXDestroyRenderContext_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXDestroyRenderContext_exit:

	return 0;
}

#if defined(SUPPORT_SERVER_SYNC_IMPL)
static IMG_INT
PVRSRVBridgeRGXKickTA3D(IMG_UINT32 ui32DispatchTableEntry,
			PVRSRV_BRIDGE_IN_RGXKICKTA3D * psRGXKickTA3DIN,
			PVRSRV_BRIDGE_OUT_RGXKICKTA3D * psRGXKickTA3DOUT,
			CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hRenderContext = psRGXKickTA3DIN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT *psRenderContextInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClientTAFenceSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClientTAFenceSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32ClientTAFenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32ClientTAFenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClientTAUpdateSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClientTAUpdateSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32ClientTAUpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32ClientTAUpdateValueInt = NULL;
	IMG_UINT32 *ui32ServerTASyncFlagsInt = NULL;
	SERVER_SYNC_PRIMITIVE **psServerTASyncsInt = NULL;
	IMG_HANDLE *hServerTASyncsInt2 = NULL;
	SYNC_PRIMITIVE_BLOCK **psClient3DFenceSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClient3DFenceSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32Client3DFenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32Client3DFenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClient3DUpdateSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClient3DUpdateSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32Client3DUpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32Client3DUpdateValueInt = NULL;
	IMG_UINT32 *ui32Server3DSyncFlagsInt = NULL;
	SERVER_SYNC_PRIMITIVE **psServer3DSyncsInt = NULL;
	IMG_HANDLE *hServer3DSyncsInt2 = NULL;
	IMG_HANDLE hPRFenceUFOSyncPrimBlock =
	    psRGXKickTA3DIN->hPRFenceUFOSyncPrimBlock;
	SYNC_PRIMITIVE_BLOCK *psPRFenceUFOSyncPrimBlockInt = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_CHAR *uiUpdateFenceName3DInt = NULL;
	IMG_BYTE *psTACmdInt = NULL;
	IMG_BYTE *ps3DPRCmdInt = NULL;
	IMG_BYTE *ps3DCmdInt = NULL;
	IMG_HANDLE hKMHWRTDataSet = psRGXKickTA3DIN->hKMHWRTDataSet;
	RGX_KM_HW_RT_DATASET *psKMHWRTDataSetInt = NULL;
	IMG_HANDLE hZSBuffer = psRGXKickTA3DIN->hZSBuffer;
	RGX_ZSBUFFER_DATA *psZSBufferInt = NULL;
	IMG_HANDLE hMSAAScratchBuffer = psRGXKickTA3DIN->hMSAAScratchBuffer;
	RGX_ZSBUFFER_DATA *psMSAAScratchBufferInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR **psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXKickTA3DIN->ui32ClientTAFenceCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32ClientTAUpdateCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32ServerTASyncPrims *
	     sizeof(SERVER_SYNC_PRIMITIVE *)) +
	    (psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3DIN->ui32Client3DFenceCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32Client3DUpdateCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32Server3DSyncPrims *
	     sizeof(SERVER_SYNC_PRIMITIVE *)) +
	    (psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE)) +
	    (psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE)) +
	    (psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE)) +
	    (psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(PMR *)) +
	    (psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) + 0;

	if (unlikely
	    (psRGXKickTA3DIN->ui32ClientTAFenceCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely
	    (psRGXKickTA3DIN->ui32ClientTAUpdateCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely
	    (psRGXKickTA3DIN->ui32ServerTASyncPrims > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely
	    (psRGXKickTA3DIN->ui32Client3DFenceCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely
	    (psRGXKickTA3DIN->ui32Client3DUpdateCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely
	    (psRGXKickTA3DIN->ui32Server3DSyncPrims > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely
	    (psRGXKickTA3DIN->ui32TACmdSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely
	    (psRGXKickTA3DIN->ui323DPRCmdSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely
	    (psRGXKickTA3DIN->ui323DCmdSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (unlikely(psRGXKickTA3DIN->ui32SyncPMRCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3DOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXKickTA3DIN), sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer = (IMG_BYTE *) psRGXKickTA3DIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXKickTA3DOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXKickTA3D_exit;
			}
		}
	}

	if (psRGXKickTA3DIN->ui32ClientTAFenceCount != 0)
	{
		psClientTAFenceSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ClientTAFenceCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClientTAFenceSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ClientTAFenceCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClientTAFenceSyncPrimBlockInt2,
		     (const void __user *)psRGXKickTA3DIN->
		     phClientTAFenceSyncPrimBlock,
		     psRGXKickTA3DIN->ui32ClientTAFenceCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32ClientTAFenceCount != 0)
	{
		ui32ClientTAFenceSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ClientTAFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientTAFenceSyncOffsetInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32ClientTAFenceSyncOffset,
		     psRGXKickTA3DIN->ui32ClientTAFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32ClientTAFenceCount != 0)
	{
		ui32ClientTAFenceValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ClientTAFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientTAFenceValueInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32ClientTAFenceValue,
		     psRGXKickTA3DIN->ui32ClientTAFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount != 0)
	{
		psClientTAUpdateSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ClientTAUpdateCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClientTAUpdateSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ClientTAUpdateCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClientTAUpdateSyncPrimBlockInt2,
		     (const void __user *)psRGXKickTA3DIN->
		     phClientTAUpdateSyncPrimBlock,
		     psRGXKickTA3DIN->ui32ClientTAUpdateCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount != 0)
	{
		ui32ClientTAUpdateSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ClientTAUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientTAUpdateSyncOffsetInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32ClientTAUpdateSyncOffset,
		     psRGXKickTA3DIN->ui32ClientTAUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount != 0)
	{
		ui32ClientTAUpdateValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ClientTAUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientTAUpdateValueInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32ClientTAUpdateValue,
		     psRGXKickTA3DIN->ui32ClientTAUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32ServerTASyncPrims != 0)
	{
		ui32ServerTASyncFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ServerTASyncFlagsInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32ServerTASyncFlags,
		     psRGXKickTA3DIN->ui32ServerTASyncPrims *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32ServerTASyncPrims != 0)
	{
		psServerTASyncsInt =
		    (SERVER_SYNC_PRIMITIVE **) (((IMG_UINT8 *) pArrayArgsBuffer)
						+ ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ServerTASyncPrims *
		    sizeof(SERVER_SYNC_PRIMITIVE *);
		hServerTASyncsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hServerTASyncsInt2,
		     (const void __user *)psRGXKickTA3DIN->phServerTASyncs,
		     psRGXKickTA3DIN->ui32ServerTASyncPrims *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32Client3DFenceCount != 0)
	{
		psClient3DFenceSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Client3DFenceCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClient3DFenceSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Client3DFenceCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClient3DFenceSyncPrimBlockInt2,
		     (const void __user *)psRGXKickTA3DIN->
		     phClient3DFenceSyncPrimBlock,
		     psRGXKickTA3DIN->ui32Client3DFenceCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32Client3DFenceCount != 0)
	{
		ui32Client3DFenceSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Client3DFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Client3DFenceSyncOffsetInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32Client3DFenceSyncOffset,
		     psRGXKickTA3DIN->ui32Client3DFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32Client3DFenceCount != 0)
	{
		ui32Client3DFenceValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Client3DFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Client3DFenceValueInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32Client3DFenceValue,
		     psRGXKickTA3DIN->ui32Client3DFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount != 0)
	{
		psClient3DUpdateSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Client3DUpdateCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClient3DUpdateSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Client3DUpdateCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClient3DUpdateSyncPrimBlockInt2,
		     (const void __user *)psRGXKickTA3DIN->
		     phClient3DUpdateSyncPrimBlock,
		     psRGXKickTA3DIN->ui32Client3DUpdateCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount != 0)
	{
		ui32Client3DUpdateSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Client3DUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Client3DUpdateSyncOffsetInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32Client3DUpdateSyncOffset,
		     psRGXKickTA3DIN->ui32Client3DUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount != 0)
	{
		ui32Client3DUpdateValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Client3DUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Client3DUpdateValueInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32Client3DUpdateValue,
		     psRGXKickTA3DIN->ui32Client3DUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32Server3DSyncPrims != 0)
	{
		ui32Server3DSyncFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Server3DSyncFlagsInt,
		     (const void __user *)psRGXKickTA3DIN->
		     pui32Server3DSyncFlags,
		     psRGXKickTA3DIN->ui32Server3DSyncPrims *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32Server3DSyncPrims != 0)
	{
		psServer3DSyncsInt =
		    (SERVER_SYNC_PRIMITIVE **) (((IMG_UINT8 *) pArrayArgsBuffer)
						+ ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Server3DSyncPrims *
		    sizeof(SERVER_SYNC_PRIMITIVE *);
		hServer3DSyncsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hServer3DSyncsInt2,
		     (const void __user *)psRGXKickTA3DIN->phServer3DSyncs,
		     psRGXKickTA3DIN->ui32Server3DSyncPrims *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}

	{
		uiUpdateFenceNameInt =
		    (IMG_CHAR *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset += PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR);
	}

	/* Copy the data over */
	if (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR) > 0)
	{
		if (OSCopyFromUser
		    (NULL, uiUpdateFenceNameInt,
		     (const void __user *)psRGXKickTA3DIN->puiUpdateFenceName,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceNameInt)[(PVRSRV_SYNC_NAME_LENGTH *
					sizeof(IMG_CHAR)) - 1] = '\0';
	}

	{
		uiUpdateFenceName3DInt =
		    (IMG_CHAR *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset += PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR);
	}

	/* Copy the data over */
	if (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR) > 0)
	{
		if (OSCopyFromUser
		    (NULL, uiUpdateFenceName3DInt,
		     (const void __user *)psRGXKickTA3DIN->puiUpdateFenceName3D,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceName3DInt)[(PVRSRV_SYNC_NAME_LENGTH *
					  sizeof(IMG_CHAR)) - 1] = '\0';
	}
	if (psRGXKickTA3DIN->ui32TACmdSize != 0)
	{
		psTACmdInt =
		    (IMG_BYTE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, psTACmdInt,
		     (const void __user *)psRGXKickTA3DIN->psTACmd,
		     psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui323DPRCmdSize != 0)
	{
		ps3DPRCmdInt =
		    (IMG_BYTE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ps3DPRCmdInt,
		     (const void __user *)psRGXKickTA3DIN->ps3DPRCmd,
		     psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui323DCmdSize != 0)
	{
		ps3DCmdInt =
		    (IMG_BYTE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ps3DCmdInt,
		     (const void __user *)psRGXKickTA3DIN->ps3DCmd,
		     psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32SyncPMRFlagsInt,
		     (const void __user *)psRGXKickTA3DIN->pui32SyncPMRFlags,
		     psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_UINT32)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}
	if (psRGXKickTA3DIN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt =
		    (PMR **) (((IMG_UINT8 *) pArrayArgsBuffer) +
			      ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(PMR *);
		hSyncPMRsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hSyncPMRsInt2,
		     (const void __user *)psRGXKickTA3DIN->phSyncPMRs,
		     psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXKickTA3DOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psRenderContextInt,
				       hRenderContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXKickTA3D_exit;
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32ClientTAFenceCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3DOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClientTAFenceSyncPrimBlockInt
						       [i],
						       hClientTAFenceSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32ClientTAUpdateCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3DOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClientTAUpdateSyncPrimBlockInt
						       [i],
						       hClientTAUpdateSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32ServerTASyncPrims; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3DOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psServerTASyncsInt[i],
						       hServerTASyncsInt2[i],
						       PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32Client3DFenceCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3DOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClient3DFenceSyncPrimBlockInt
						       [i],
						       hClient3DFenceSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32Client3DUpdateCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3DOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClient3DUpdateSyncPrimBlockInt
						       [i],
						       hClient3DUpdateSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32Server3DSyncPrims; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3DOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psServer3DSyncsInt[i],
						       hServer3DSyncsInt2[i],
						       PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D_exit;
			}
		}
	}

	/* Look up the address from the handle */
	psRGXKickTA3DOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPRFenceUFOSyncPrimBlockInt,
				       hPRFenceUFOSyncPrimBlock,
				       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
				       IMG_TRUE);
	if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXKickTA3D_exit;
	}

	if (psRGXKickTA3DIN->hKMHWRTDataSet)
	{
		/* Look up the address from the handle */
		psRGXKickTA3DOUT->eError =
		    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
					       (void **)&psKMHWRTDataSetInt,
					       hKMHWRTDataSet,
					       PVRSRV_HANDLE_TYPE_RGX_KM_HW_RT_DATASET,
					       IMG_TRUE);
		if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
		{
			UnlockHandle(psConnection->psHandleBase);
			goto RGXKickTA3D_exit;
		}
	}

	if (psRGXKickTA3DIN->hZSBuffer)
	{
		/* Look up the address from the handle */
		psRGXKickTA3DOUT->eError =
		    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
					       (void **)&psZSBufferInt,
					       hZSBuffer,
					       PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
					       IMG_TRUE);
		if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
		{
			UnlockHandle(psConnection->psHandleBase);
			goto RGXKickTA3D_exit;
		}
	}

	if (psRGXKickTA3DIN->hMSAAScratchBuffer)
	{
		/* Look up the address from the handle */
		psRGXKickTA3DOUT->eError =
		    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
					       (void **)&psMSAAScratchBufferInt,
					       hMSAAScratchBuffer,
					       PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
					       IMG_TRUE);
		if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
		{
			UnlockHandle(psConnection->psHandleBase);
			goto RGXKickTA3D_exit;
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32SyncPMRCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3DOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psSyncPMRsInt[i],
						       hSyncPMRsInt2[i],
						       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3DOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXKickTA3DOUT->eError =
	    PVRSRVRGXKickTA3DKM(psRenderContextInt,
				psRGXKickTA3DIN->ui32ClientCacheOpSeqNum,
				psRGXKickTA3DIN->ui32ClientTAFenceCount,
				psClientTAFenceSyncPrimBlockInt,
				ui32ClientTAFenceSyncOffsetInt,
				ui32ClientTAFenceValueInt,
				psRGXKickTA3DIN->ui32ClientTAUpdateCount,
				psClientTAUpdateSyncPrimBlockInt,
				ui32ClientTAUpdateSyncOffsetInt,
				ui32ClientTAUpdateValueInt,
				psRGXKickTA3DIN->ui32ServerTASyncPrims,
				ui32ServerTASyncFlagsInt,
				psServerTASyncsInt,
				psRGXKickTA3DIN->ui32Client3DFenceCount,
				psClient3DFenceSyncPrimBlockInt,
				ui32Client3DFenceSyncOffsetInt,
				ui32Client3DFenceValueInt,
				psRGXKickTA3DIN->ui32Client3DUpdateCount,
				psClient3DUpdateSyncPrimBlockInt,
				ui32Client3DUpdateSyncOffsetInt,
				ui32Client3DUpdateValueInt,
				psRGXKickTA3DIN->ui32Server3DSyncPrims,
				ui32Server3DSyncFlagsInt,
				psServer3DSyncsInt,
				psPRFenceUFOSyncPrimBlockInt,
				psRGXKickTA3DIN->ui32FRFenceUFOSyncOffset,
				psRGXKickTA3DIN->ui32FRFenceValue,
				psRGXKickTA3DIN->hCheckFence,
				psRGXKickTA3DIN->hUpdateTimeline,
				&psRGXKickTA3DOUT->hUpdateFence,
				uiUpdateFenceNameInt,
				psRGXKickTA3DIN->hCheckFence3D,
				psRGXKickTA3DIN->hUpdateTimeline3D,
				&psRGXKickTA3DOUT->hUpdateFence3D,
				uiUpdateFenceName3DInt,
				psRGXKickTA3DIN->ui32TACmdSize,
				psTACmdInt,
				psRGXKickTA3DIN->ui323DPRCmdSize,
				ps3DPRCmdInt,
				psRGXKickTA3DIN->ui323DCmdSize,
				ps3DCmdInt,
				psRGXKickTA3DIN->ui32ExtJobRef,
				psRGXKickTA3DIN->bbKickTA,
				psRGXKickTA3DIN->bbKickPR,
				psRGXKickTA3DIN->bbKick3D,
				psRGXKickTA3DIN->bbAbort,
				psRGXKickTA3DIN->ui32PDumpFlags,
				psKMHWRTDataSetInt,
				psZSBufferInt,
				psMSAAScratchBufferInt,
				psRGXKickTA3DIN->ui32SyncPMRCount,
				ui32SyncPMRFlagsInt,
				psSyncPMRsInt,
				psRGXKickTA3DIN->ui32RenderTargetSize,
				psRGXKickTA3DIN->ui32NumberOfDrawCalls,
				psRGXKickTA3DIN->ui32NumberOfIndices,
				psRGXKickTA3DIN->ui32NumberOfMRTs,
				psRGXKickTA3DIN->ui64Deadline);

RGXKickTA3D_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psRenderContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hRenderContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
	}

	if (hClientTAFenceSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32ClientTAFenceCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClientTAFenceSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClientTAFenceSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hClientTAUpdateSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32ClientTAUpdateCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClientTAUpdateSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClientTAUpdateSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hServerTASyncsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32ServerTASyncPrims; i++)
		{

			/* Unreference the previously looked up handle */
			if (hServerTASyncsInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hServerTASyncsInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
			}
		}
	}

	if (hClient3DFenceSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32Client3DFenceCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClient3DFenceSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClient3DFenceSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hClient3DUpdateSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32Client3DUpdateCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClient3DUpdateSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClient3DUpdateSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hServer3DSyncsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32Server3DSyncPrims; i++)
		{

			/* Unreference the previously looked up handle */
			if (hServer3DSyncsInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hServer3DSyncsInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
			}
		}
	}

	/* Unreference the previously looked up handle */
	if (psPRFenceUFOSyncPrimBlockInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPRFenceUFOSyncPrimBlock,
					    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
	}

	if (psRGXKickTA3DIN->hKMHWRTDataSet)
	{

		/* Unreference the previously looked up handle */
		if (psKMHWRTDataSetInt)
		{
			PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
						    hKMHWRTDataSet,
						    PVRSRV_HANDLE_TYPE_RGX_KM_HW_RT_DATASET);
		}
	}

	if (psRGXKickTA3DIN->hZSBuffer)
	{

		/* Unreference the previously looked up handle */
		if (psZSBufferInt)
		{
			PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
						    hZSBuffer,
						    PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
		}
	}

	if (psRGXKickTA3DIN->hMSAAScratchBuffer)
	{

		/* Unreference the previously looked up handle */
		if (psMSAAScratchBufferInt)
		{
			PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
						    hMSAAScratchBuffer,
						    PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
		}
	}

	if (hSyncPMRsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3DIN->ui32SyncPMRCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hSyncPMRsInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hSyncPMRsInt2[i],
							    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
			}
		}
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

#else
#define PVRSRVBridgeRGXKickTA3D NULL
#endif

static IMG_INT
PVRSRVBridgeRGXSetRenderContextPriority(IMG_UINT32 ui32DispatchTableEntry,
					PVRSRV_BRIDGE_IN_RGXSETRENDERCONTEXTPRIORITY
					* psRGXSetRenderContextPriorityIN,
					PVRSRV_BRIDGE_OUT_RGXSETRENDERCONTEXTPRIORITY
					* psRGXSetRenderContextPriorityOUT,
					CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hRenderContext =
	    psRGXSetRenderContextPriorityIN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT *psRenderContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXSetRenderContextPriorityOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psRenderContextInt,
				       hRenderContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXSetRenderContextPriorityOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXSetRenderContextPriority_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXSetRenderContextPriorityOUT->eError =
	    PVRSRVRGXSetRenderContextPriorityKM(psConnection,
						OSGetDevNode(psConnection),
						psRenderContextInt,
						psRGXSetRenderContextPriorityIN->
						ui32Priority);

RGXSetRenderContextPriority_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psRenderContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hRenderContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXGetLastRenderContextResetReason(IMG_UINT32
					       ui32DispatchTableEntry,
					       PVRSRV_BRIDGE_IN_RGXGETLASTRENDERCONTEXTRESETREASON
					       *
					       psRGXGetLastRenderContextResetReasonIN,
					       PVRSRV_BRIDGE_OUT_RGXGETLASTRENDERCONTEXTRESETREASON
					       *
					       psRGXGetLastRenderContextResetReasonOUT,
					       CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hRenderContext =
	    psRGXGetLastRenderContextResetReasonIN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT *psRenderContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXGetLastRenderContextResetReasonOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psRenderContextInt,
				       hRenderContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
				       IMG_TRUE);
	if (unlikely
	    (psRGXGetLastRenderContextResetReasonOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXGetLastRenderContextResetReason_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXGetLastRenderContextResetReasonOUT->eError =
	    PVRSRVRGXGetLastRenderContextResetReasonKM(psRenderContextInt,
						       &psRGXGetLastRenderContextResetReasonOUT->
						       ui32LastResetReason,
						       &psRGXGetLastRenderContextResetReasonOUT->
						       ui32LastResetJobRef);

RGXGetLastRenderContextResetReason_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psRenderContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hRenderContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXRenderContextStalled(IMG_UINT32 ui32DispatchTableEntry,
				    PVRSRV_BRIDGE_IN_RGXRENDERCONTEXTSTALLED *
				    psRGXRenderContextStalledIN,
				    PVRSRV_BRIDGE_OUT_RGXRENDERCONTEXTSTALLED *
				    psRGXRenderContextStalledOUT,
				    CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hRenderContext = psRGXRenderContextStalledIN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT *psRenderContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXRenderContextStalledOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psRenderContextInt,
				       hRenderContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXRenderContextStalledOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXRenderContextStalled_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXRenderContextStalledOUT->eError =
	    RGXRenderContextStalledKM(psRenderContextInt);

RGXRenderContextStalled_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psRenderContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hRenderContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

#if !defined(SUPPORT_SERVER_SYNC_IMPL)
static IMG_INT
PVRSRVBridgeRGXKickTA3D2(IMG_UINT32 ui32DispatchTableEntry,
			 PVRSRV_BRIDGE_IN_RGXKICKTA3D2 * psRGXKickTA3D2IN,
			 PVRSRV_BRIDGE_OUT_RGXKICKTA3D2 * psRGXKickTA3D2OUT,
			 CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hRenderContext = psRGXKickTA3D2IN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT *psRenderContextInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClientTAFenceSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClientTAFenceSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32ClientTAFenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32ClientTAFenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClientTAUpdateSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClientTAUpdateSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32ClientTAUpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32ClientTAUpdateValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClient3DFenceSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClient3DFenceSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32Client3DFenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32Client3DFenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClient3DUpdateSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClient3DUpdateSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32Client3DUpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32Client3DUpdateValueInt = NULL;
	IMG_HANDLE hPRFenceUFOSyncPrimBlock =
	    psRGXKickTA3D2IN->hPRFenceUFOSyncPrimBlock;
	SYNC_PRIMITIVE_BLOCK *psPRFenceUFOSyncPrimBlockInt = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_CHAR *uiUpdateFenceName3DInt = NULL;
	IMG_BYTE *psTACmdInt = NULL;
	IMG_BYTE *ps3DPRCmdInt = NULL;
	IMG_BYTE *ps3DCmdInt = NULL;
	IMG_HANDLE hKMHWRTDataSet = psRGXKickTA3D2IN->hKMHWRTDataSet;
	RGX_KM_HW_RT_DATASET *psKMHWRTDataSetInt = NULL;
	IMG_HANDLE hZSBuffer = psRGXKickTA3D2IN->hZSBuffer;
	RGX_ZSBUFFER_DATA *psZSBufferInt = NULL;
	IMG_HANDLE hMSAAScratchBuffer = psRGXKickTA3D2IN->hMSAAScratchBuffer;
	RGX_ZSBUFFER_DATA *psMSAAScratchBufferInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR **psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXKickTA3D2IN->ui32ClientTAFenceCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickTA3D2IN->ui32ClientTAFenceCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3D2IN->ui32ClientTAFenceCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3D2IN->ui32ClientTAFenceCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3D2IN->ui32ClientTAUpdateCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickTA3D2IN->ui32ClientTAUpdateCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3D2IN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3D2IN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3D2IN->ui32Client3DFenceCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickTA3D2IN->ui32Client3DFenceCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3D2IN->ui32Client3DFenceCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3D2IN->ui32Client3DFenceCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3D2IN->ui32Client3DUpdateCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickTA3D2IN->ui32Client3DUpdateCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickTA3D2IN->ui32Client3DUpdateCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3D2IN->ui32Client3DUpdateCount * sizeof(IMG_UINT32)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (psRGXKickTA3D2IN->ui32TACmdSize * sizeof(IMG_BYTE)) +
	    (psRGXKickTA3D2IN->ui323DPRCmdSize * sizeof(IMG_BYTE)) +
	    (psRGXKickTA3D2IN->ui323DCmdSize * sizeof(IMG_BYTE)) +
	    (psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(IMG_UINT32)) +
	    (psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(PMR *)) +
	    (psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) + 0;

	if (unlikely
	    (psRGXKickTA3D2IN->ui32ClientTAFenceCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3D2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D2_exit;
	}

	if (unlikely
	    (psRGXKickTA3D2IN->ui32ClientTAUpdateCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3D2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D2_exit;
	}

	if (unlikely
	    (psRGXKickTA3D2IN->ui32Client3DFenceCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3D2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D2_exit;
	}

	if (unlikely
	    (psRGXKickTA3D2IN->ui32Client3DUpdateCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3D2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D2_exit;
	}

	if (unlikely
	    (psRGXKickTA3D2IN->ui32TACmdSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXKickTA3D2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D2_exit;
	}

	if (unlikely
	    (psRGXKickTA3D2IN->ui323DPRCmdSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXKickTA3D2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D2_exit;
	}

	if (unlikely
	    (psRGXKickTA3D2IN->ui323DCmdSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXKickTA3D2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D2_exit;
	}

	if (unlikely
	    (psRGXKickTA3D2IN->ui32SyncPMRCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXKickTA3D2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickTA3D2_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXKickTA3D2IN), sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer = (IMG_BYTE *) psRGXKickTA3D2IN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXKickTA3D2OUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXKickTA3D2_exit;
			}
		}
	}

	if (psRGXKickTA3D2IN->ui32ClientTAFenceCount != 0)
	{
		psClientTAFenceSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32ClientTAFenceCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClientTAFenceSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32ClientTAFenceCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32ClientTAFenceCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClientTAFenceSyncPrimBlockInt2,
		     (const void __user *)psRGXKickTA3D2IN->
		     phClientTAFenceSyncPrimBlock,
		     psRGXKickTA3D2IN->ui32ClientTAFenceCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32ClientTAFenceCount != 0)
	{
		ui32ClientTAFenceSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32ClientTAFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32ClientTAFenceCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientTAFenceSyncOffsetInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     pui32ClientTAFenceSyncOffset,
		     psRGXKickTA3D2IN->ui32ClientTAFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32ClientTAFenceCount != 0)
	{
		ui32ClientTAFenceValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32ClientTAFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32ClientTAFenceCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientTAFenceValueInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     pui32ClientTAFenceValue,
		     psRGXKickTA3D2IN->ui32ClientTAFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32ClientTAUpdateCount != 0)
	{
		psClientTAUpdateSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32ClientTAUpdateCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClientTAUpdateSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32ClientTAUpdateCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32ClientTAUpdateCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClientTAUpdateSyncPrimBlockInt2,
		     (const void __user *)psRGXKickTA3D2IN->
		     phClientTAUpdateSyncPrimBlock,
		     psRGXKickTA3D2IN->ui32ClientTAUpdateCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32ClientTAUpdateCount != 0)
	{
		ui32ClientTAUpdateSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32ClientTAUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientTAUpdateSyncOffsetInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     pui32ClientTAUpdateSyncOffset,
		     psRGXKickTA3D2IN->ui32ClientTAUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32ClientTAUpdateCount != 0)
	{
		ui32ClientTAUpdateValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32ClientTAUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientTAUpdateValueInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     pui32ClientTAUpdateValue,
		     psRGXKickTA3D2IN->ui32ClientTAUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32Client3DFenceCount != 0)
	{
		psClient3DFenceSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32Client3DFenceCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClient3DFenceSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32Client3DFenceCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32Client3DFenceCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClient3DFenceSyncPrimBlockInt2,
		     (const void __user *)psRGXKickTA3D2IN->
		     phClient3DFenceSyncPrimBlock,
		     psRGXKickTA3D2IN->ui32Client3DFenceCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32Client3DFenceCount != 0)
	{
		ui32Client3DFenceSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32Client3DFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32Client3DFenceCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Client3DFenceSyncOffsetInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     pui32Client3DFenceSyncOffset,
		     psRGXKickTA3D2IN->ui32Client3DFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32Client3DFenceCount != 0)
	{
		ui32Client3DFenceValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32Client3DFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32Client3DFenceCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Client3DFenceValueInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     pui32Client3DFenceValue,
		     psRGXKickTA3D2IN->ui32Client3DFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32Client3DUpdateCount != 0)
	{
		psClient3DUpdateSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32Client3DUpdateCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClient3DUpdateSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32Client3DUpdateCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32Client3DUpdateCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClient3DUpdateSyncPrimBlockInt2,
		     (const void __user *)psRGXKickTA3D2IN->
		     phClient3DUpdateSyncPrimBlock,
		     psRGXKickTA3D2IN->ui32Client3DUpdateCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32Client3DUpdateCount != 0)
	{
		ui32Client3DUpdateSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32Client3DUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32Client3DUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Client3DUpdateSyncOffsetInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     pui32Client3DUpdateSyncOffset,
		     psRGXKickTA3D2IN->ui32Client3DUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32Client3DUpdateCount != 0)
	{
		ui32Client3DUpdateValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32Client3DUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32Client3DUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32Client3DUpdateValueInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     pui32Client3DUpdateValue,
		     psRGXKickTA3D2IN->ui32Client3DUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}

	{
		uiUpdateFenceNameInt =
		    (IMG_CHAR *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset += PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR);
	}

	/* Copy the data over */
	if (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR) > 0)
	{
		if (OSCopyFromUser
		    (NULL, uiUpdateFenceNameInt,
		     (const void __user *)psRGXKickTA3D2IN->puiUpdateFenceName,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceNameInt)[(PVRSRV_SYNC_NAME_LENGTH *
					sizeof(IMG_CHAR)) - 1] = '\0';
	}

	{
		uiUpdateFenceName3DInt =
		    (IMG_CHAR *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset += PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR);
	}

	/* Copy the data over */
	if (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR) > 0)
	{
		if (OSCopyFromUser
		    (NULL, uiUpdateFenceName3DInt,
		     (const void __user *)psRGXKickTA3D2IN->
		     puiUpdateFenceName3D,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceName3DInt)[(PVRSRV_SYNC_NAME_LENGTH *
					  sizeof(IMG_CHAR)) - 1] = '\0';
	}
	if (psRGXKickTA3D2IN->ui32TACmdSize != 0)
	{
		psTACmdInt =
		    (IMG_BYTE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32TACmdSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32TACmdSize * sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, psTACmdInt,
		     (const void __user *)psRGXKickTA3D2IN->psTACmd,
		     psRGXKickTA3D2IN->ui32TACmdSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui323DPRCmdSize != 0)
	{
		ps3DPRCmdInt =
		    (IMG_BYTE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui323DPRCmdSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui323DPRCmdSize * sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ps3DPRCmdInt,
		     (const void __user *)psRGXKickTA3D2IN->ps3DPRCmd,
		     psRGXKickTA3D2IN->ui323DPRCmdSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui323DCmdSize != 0)
	{
		ps3DCmdInt =
		    (IMG_BYTE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui323DCmdSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui323DCmdSize * sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ps3DCmdInt,
		     (const void __user *)psRGXKickTA3D2IN->ps3DCmd,
		     psRGXKickTA3D2IN->ui323DCmdSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32SyncPMRFlagsInt,
		     (const void __user *)psRGXKickTA3D2IN->pui32SyncPMRFlags,
		     psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(IMG_UINT32)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}
	if (psRGXKickTA3D2IN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt =
		    (PMR **) (((IMG_UINT8 *) pArrayArgsBuffer) +
			      ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(PMR *);
		hSyncPMRsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hSyncPMRsInt2,
		     (const void __user *)psRGXKickTA3D2IN->phSyncPMRs,
		     psRGXKickTA3D2IN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) !=
		    PVRSRV_OK)
		{
			psRGXKickTA3D2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickTA3D2_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXKickTA3D2OUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psRenderContextInt,
				       hRenderContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXKickTA3D2_exit;
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32ClientTAFenceCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3D2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClientTAFenceSyncPrimBlockInt
						       [i],
						       hClientTAFenceSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D2_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32ClientTAUpdateCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3D2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClientTAUpdateSyncPrimBlockInt
						       [i],
						       hClientTAUpdateSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D2_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32Client3DFenceCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3D2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClient3DFenceSyncPrimBlockInt
						       [i],
						       hClient3DFenceSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D2_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32Client3DUpdateCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3D2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClient3DUpdateSyncPrimBlockInt
						       [i],
						       hClient3DUpdateSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D2_exit;
			}
		}
	}

	/* Look up the address from the handle */
	psRGXKickTA3D2OUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psPRFenceUFOSyncPrimBlockInt,
				       hPRFenceUFOSyncPrimBlock,
				       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
				       IMG_TRUE);
	if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXKickTA3D2_exit;
	}

	if (psRGXKickTA3D2IN->hKMHWRTDataSet)
	{
		/* Look up the address from the handle */
		psRGXKickTA3D2OUT->eError =
		    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
					       (void **)&psKMHWRTDataSetInt,
					       hKMHWRTDataSet,
					       PVRSRV_HANDLE_TYPE_RGX_KM_HW_RT_DATASET,
					       IMG_TRUE);
		if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
		{
			UnlockHandle(psConnection->psHandleBase);
			goto RGXKickTA3D2_exit;
		}
	}

	if (psRGXKickTA3D2IN->hZSBuffer)
	{
		/* Look up the address from the handle */
		psRGXKickTA3D2OUT->eError =
		    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
					       (void **)&psZSBufferInt,
					       hZSBuffer,
					       PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
					       IMG_TRUE);
		if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
		{
			UnlockHandle(psConnection->psHandleBase);
			goto RGXKickTA3D2_exit;
		}
	}

	if (psRGXKickTA3D2IN->hMSAAScratchBuffer)
	{
		/* Look up the address from the handle */
		psRGXKickTA3D2OUT->eError =
		    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
					       (void **)&psMSAAScratchBufferInt,
					       hMSAAScratchBuffer,
					       PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
					       IMG_TRUE);
		if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
		{
			UnlockHandle(psConnection->psHandleBase);
			goto RGXKickTA3D2_exit;
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32SyncPMRCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickTA3D2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psSyncPMRsInt[i],
						       hSyncPMRsInt2[i],
						       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
						       IMG_TRUE);
			if (unlikely(psRGXKickTA3D2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickTA3D2_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXKickTA3D2OUT->eError =
	    PVRSRVRGXKickTA3DKM(psRenderContextInt,
				psRGXKickTA3D2IN->ui32ClientCacheOpSeqNum,
				psRGXKickTA3D2IN->ui32ClientTAFenceCount,
				psClientTAFenceSyncPrimBlockInt,
				ui32ClientTAFenceSyncOffsetInt,
				ui32ClientTAFenceValueInt,
				psRGXKickTA3D2IN->ui32ClientTAUpdateCount,
				psClientTAUpdateSyncPrimBlockInt,
				ui32ClientTAUpdateSyncOffsetInt,
				ui32ClientTAUpdateValueInt,
				psRGXKickTA3D2IN->ui32Client3DFenceCount,
				psClient3DFenceSyncPrimBlockInt,
				ui32Client3DFenceSyncOffsetInt,
				ui32Client3DFenceValueInt,
				psRGXKickTA3D2IN->ui32Client3DUpdateCount,
				psClient3DUpdateSyncPrimBlockInt,
				ui32Client3DUpdateSyncOffsetInt,
				ui32Client3DUpdateValueInt,
				psPRFenceUFOSyncPrimBlockInt,
				psRGXKickTA3D2IN->ui32FRFenceUFOSyncOffset,
				psRGXKickTA3D2IN->ui32FRFenceValue,
				psRGXKickTA3D2IN->hCheckFence,
				psRGXKickTA3D2IN->hUpdateTimeline,
				&psRGXKickTA3D2OUT->hUpdateFence,
				uiUpdateFenceNameInt,
				psRGXKickTA3D2IN->hCheckFence3D,
				psRGXKickTA3D2IN->hUpdateTimeline3D,
				&psRGXKickTA3D2OUT->hUpdateFence3D,
				uiUpdateFenceName3DInt,
				psRGXKickTA3D2IN->ui32TACmdSize,
				psTACmdInt,
				psRGXKickTA3D2IN->ui323DPRCmdSize,
				ps3DPRCmdInt,
				psRGXKickTA3D2IN->ui323DCmdSize,
				ps3DCmdInt,
				psRGXKickTA3D2IN->ui32ExtJobRef,
				psRGXKickTA3D2IN->bbKickTA,
				psRGXKickTA3D2IN->bbKickPR,
				psRGXKickTA3D2IN->bbKick3D,
				psRGXKickTA3D2IN->bbAbort,
				psRGXKickTA3D2IN->ui32PDumpFlags,
				psKMHWRTDataSetInt,
				psZSBufferInt,
				psMSAAScratchBufferInt,
				psRGXKickTA3D2IN->ui32SyncPMRCount,
				ui32SyncPMRFlagsInt,
				psSyncPMRsInt,
				psRGXKickTA3D2IN->ui32RenderTargetSize,
				psRGXKickTA3D2IN->ui32NumberOfDrawCalls,
				psRGXKickTA3D2IN->ui32NumberOfIndices,
				psRGXKickTA3D2IN->ui32NumberOfMRTs,
				psRGXKickTA3D2IN->ui64Deadline);

RGXKickTA3D2_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psRenderContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hRenderContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
	}

	if (hClientTAFenceSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32ClientTAFenceCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClientTAFenceSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClientTAFenceSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hClientTAUpdateSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32ClientTAUpdateCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClientTAUpdateSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClientTAUpdateSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hClient3DFenceSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32Client3DFenceCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClient3DFenceSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClient3DFenceSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hClient3DUpdateSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32Client3DUpdateCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClient3DUpdateSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClient3DUpdateSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	/* Unreference the previously looked up handle */
	if (psPRFenceUFOSyncPrimBlockInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPRFenceUFOSyncPrimBlock,
					    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
	}

	if (psRGXKickTA3D2IN->hKMHWRTDataSet)
	{

		/* Unreference the previously looked up handle */
		if (psKMHWRTDataSetInt)
		{
			PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
						    hKMHWRTDataSet,
						    PVRSRV_HANDLE_TYPE_RGX_KM_HW_RT_DATASET);
		}
	}

	if (psRGXKickTA3D2IN->hZSBuffer)
	{

		/* Unreference the previously looked up handle */
		if (psZSBufferInt)
		{
			PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
						    hZSBuffer,
						    PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
		}
	}

	if (psRGXKickTA3D2IN->hMSAAScratchBuffer)
	{

		/* Unreference the previously looked up handle */
		if (psMSAAScratchBufferInt)
		{
			PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
						    hMSAAScratchBuffer,
						    PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
		}
	}

	if (hSyncPMRsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickTA3D2IN->ui32SyncPMRCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hSyncPMRsInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hSyncPMRsInt2[i],
							    PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
			}
		}
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize == ui32NextOffset);

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

#else
#define PVRSRVBridgeRGXKickTA3D2 NULL
#endif

static IMG_INT
PVRSRVBridgeRGXSetRenderContextProperty(IMG_UINT32 ui32DispatchTableEntry,
					PVRSRV_BRIDGE_IN_RGXSETRENDERCONTEXTPROPERTY
					* psRGXSetRenderContextPropertyIN,
					PVRSRV_BRIDGE_OUT_RGXSETRENDERCONTEXTPROPERTY
					* psRGXSetRenderContextPropertyOUT,
					CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hRenderContext =
	    psRGXSetRenderContextPropertyIN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT *psRenderContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXSetRenderContextPropertyOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psRenderContextInt,
				       hRenderContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXSetRenderContextPropertyOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXSetRenderContextProperty_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXSetRenderContextPropertyOUT->eError =
	    PVRSRVRGXSetRenderContextPropertyKM(psRenderContextInt,
						psRGXSetRenderContextPropertyIN->
						ui32Property,
						psRGXSetRenderContextPropertyIN->
						ui64Input,
						&psRGXSetRenderContextPropertyOUT->
						ui64Output);

RGXSetRenderContextProperty_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psRenderContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hRenderContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR InitRGXTA3DBridge(void);
PVRSRV_ERROR DeinitRGXTA3DBridge(void);

/*
 * Register all RGXTA3D functions with services
 */
PVRSRV_ERROR InitRGXTA3DBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATASET,
			      PVRSRVBridgeRGXCreateHWRTDataSet, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATASET,
			      PVRSRVBridgeRGXDestroyHWRTDataSet, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXCREATEZSBUFFER,
			      PVRSRVBridgeRGXCreateZSBuffer, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYZSBUFFER,
			      PVRSRVBridgeRGXDestroyZSBuffer, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXPOPULATEZSBUFFER,
			      PVRSRVBridgeRGXPopulateZSBuffer, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXUNPOPULATEZSBUFFER,
			      PVRSRVBridgeRGXUnpopulateZSBuffer, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST,
			      PVRSRVBridgeRGXCreateFreeList, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST,
			      PVRSRVBridgeRGXDestroyFreeList, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT,
			      PVRSRVBridgeRGXCreateRenderContext, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT,
			      PVRSRVBridgeRGXDestroyRenderContext, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXKICKTA3D,
			      PVRSRVBridgeRGXKickTA3D, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXSETRENDERCONTEXTPRIORITY,
			      PVRSRVBridgeRGXSetRenderContextPriority, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXGETLASTRENDERCONTEXTRESETREASON,
			      PVRSRVBridgeRGXGetLastRenderContextResetReason,
			      NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXRENDERCONTEXTSTALLED,
			      PVRSRVBridgeRGXRenderContextStalled, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXKICKTA3D2,
			      PVRSRVBridgeRGXKickTA3D2, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
			      PVRSRV_BRIDGE_RGXTA3D_RGXSETRENDERCONTEXTPROPERTY,
			      PVRSRVBridgeRGXSetRenderContextProperty, NULL);

	return PVRSRV_OK;
}

/*
 * Unregister all rgxta3d functions with services
 */
PVRSRV_ERROR DeinitRGXTA3DBridge(void)
{

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATASET);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATASET);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXCREATEZSBUFFER);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYZSBUFFER);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXPOPULATEZSBUFFER);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXUNPOPULATEZSBUFFER);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXKICKTA3D);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXSETRENDERCONTEXTPRIORITY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXGETLASTRENDERCONTEXTRESETREASON);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXRENDERCONTEXTSTALLED);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXKICKTA3D2);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D,
				PVRSRV_BRIDGE_RGXTA3D_RGXSETRENDERCONTEXTPROPERTY);

	return PVRSRV_OK;
}
