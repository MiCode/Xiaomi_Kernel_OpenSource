/*************************************************************************/ /*!
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
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "rgxta3d.h"


#include "common_rgxta3d_bridge.h"

#include "allocmem.h"
#include "pvr_debug.h"
#include "connection_server.h"
#include "pvr_bridge.h"
#include "rgx_bridge.h"
#include "srvcore.h"
#include "handle.h"

#include <linux/slab.h>





/* ***************************************************************************
 * Server-side bridge entry points
 */
 
static IMG_INT
PVRSRVBridgeRGXCreateHWRTData(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXCREATEHWRTDATA *psRGXCreateHWRTDataIN,
					  PVRSRV_BRIDGE_OUT_RGXCREATEHWRTDATA *psRGXCreateHWRTDataOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_FREELIST * *psapsFreeListsInt = NULL;
	IMG_HANDLE *hapsFreeListsInt2 = NULL;
	RGX_RTDATA_CLEANUP_DATA * psCleanupCookieInt = NULL;
	DEVMEM_MEMDESC * psRTACtlMemDescInt = NULL;
	DEVMEM_MEMDESC * pssHWRTDataMemDescInt = NULL;



	psRGXCreateHWRTDataOUT->hCleanupCookie = NULL;

	
	{
		psapsFreeListsInt = OSAllocZMemNoStats(RGXFW_MAX_FREELISTS * sizeof(RGX_FREELIST *));
		if (!psapsFreeListsInt)
		{
			psRGXCreateHWRTDataOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXCreateHWRTData_exit;
		}
		hapsFreeListsInt2 = OSAllocMemNoStats(RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE));
		if (!hapsFreeListsInt2)
		{
			psRGXCreateHWRTDataOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXCreateHWRTData_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXCreateHWRTDataIN->phapsFreeLists, RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hapsFreeListsInt2, psRGXCreateHWRTDataIN->phapsFreeLists,
				RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXCreateHWRTDataOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXCreateHWRTData_exit;
			}






	{
		IMG_UINT32 i;

		for (i=0;i<RGXFW_MAX_FREELISTS;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXCreateHWRTDataOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psapsFreeListsInt[i],
											hapsFreeListsInt2[i],
											PVRSRV_HANDLE_TYPE_RGX_FREELIST,
											IMG_TRUE);
					if(psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateHWRTData_exit;
					}
				}
		}
	}

	psRGXCreateHWRTDataOUT->eError =
		RGXCreateHWRTData(psConnection, OSGetDevData(psConnection),
					psRGXCreateHWRTDataIN->ui32RenderTarget,
					psRGXCreateHWRTDataIN->sPMMlistDevVAddr,
					psRGXCreateHWRTDataIN->sVFPPageTableAddr,
					psapsFreeListsInt,
					&psCleanupCookieInt,
					&psRTACtlMemDescInt,
					psRGXCreateHWRTDataIN->ui32PPPScreen,
					psRGXCreateHWRTDataIN->ui32PPPGridOffset,
					psRGXCreateHWRTDataIN->ui64PPPMultiSampleCtl,
					psRGXCreateHWRTDataIN->ui32TPCStride,
					psRGXCreateHWRTDataIN->sTailPtrsDevVAddr,
					psRGXCreateHWRTDataIN->ui32TPCSize,
					psRGXCreateHWRTDataIN->ui32TEScreen,
					psRGXCreateHWRTDataIN->ui32TEAA,
					psRGXCreateHWRTDataIN->ui32TEMTILE1,
					psRGXCreateHWRTDataIN->ui32TEMTILE2,
					psRGXCreateHWRTDataIN->ui32MTileStride,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeLowerX,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeLowerY,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeUpperX,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeUpperY,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeScaleX,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeScaleY,
					psRGXCreateHWRTDataIN->ui16MaxRTs,
					&pssHWRTDataMemDescInt,
					&psRGXCreateHWRTDataOUT->ui32FWHWRTData);
	/* Exit early if bridged call fails */
	if(psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateHWRTData_exit;
	}






	psRGXCreateHWRTDataOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRGXCreateHWRTDataOUT->hCleanupCookie,
							(void *) psCleanupCookieInt,
							PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&RGXDestroyHWRTData);
	if (psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateHWRTData_exit;
	}






	psRGXCreateHWRTDataOUT->eError = PVRSRVAllocSubHandle(psConnection->psHandleBase,

							&psRGXCreateHWRTDataOUT->hRTACtlMemDesc,
							(void *) psRTACtlMemDescInt,
							PVRSRV_HANDLE_TYPE_RGX_FW_MEMDESC,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							,psRGXCreateHWRTDataOUT->hCleanupCookie);
	if (psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateHWRTData_exit;
	}






	psRGXCreateHWRTDataOUT->eError = PVRSRVAllocSubHandle(psConnection->psHandleBase,

							&psRGXCreateHWRTDataOUT->hsHWRTDataMemDesc,
							(void *) pssHWRTDataMemDescInt,
							PVRSRV_HANDLE_TYPE_RGX_FW_MEMDESC,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							,psRGXCreateHWRTDataOUT->hCleanupCookie);
	if (psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateHWRTData_exit;
	}




RGXCreateHWRTData_exit:






	{
		IMG_UINT32 i;

		for (i=0;i<RGXFW_MAX_FREELISTS;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psapsFreeListsInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hapsFreeListsInt2[i],
											PVRSRV_HANDLE_TYPE_RGX_FREELIST);
						}
				}
		}
	}

	if (psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		if (psRGXCreateHWRTDataOUT->hCleanupCookie)
		{


			PVRSRV_ERROR eError = PVRSRVReleaseHandle(psConnection->psHandleBase,
						(IMG_HANDLE) psRGXCreateHWRTDataOUT->hCleanupCookie,
						PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP);

			/* Releasing the handle should free/destroy/release the resource. This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

			/* Avoid freeing/destroying/releasing the resource a second time below */
			psCleanupCookieInt = NULL;
		}


		if (psCleanupCookieInt)
		{
			RGXDestroyHWRTData(psCleanupCookieInt);
		}
	}

	if (psapsFreeListsInt)
		OSFreeMemNoStats(psapsFreeListsInt);
	if (hapsFreeListsInt2)
		OSFreeMemNoStats(hapsFreeListsInt2);

	return 0;
}


static IMG_INT
PVRSRVBridgeRGXDestroyHWRTData(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXDESTROYHWRTDATA *psRGXDestroyHWRTDataIN,
					  PVRSRV_BRIDGE_OUT_RGXDESTROYHWRTDATA *psRGXDestroyHWRTDataOUT,
					 CONNECTION_DATA *psConnection)
{












	psRGXDestroyHWRTDataOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyHWRTDataIN->hCleanupCookie,
					PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP);
	if ((psRGXDestroyHWRTDataOUT->eError != PVRSRV_OK) && (psRGXDestroyHWRTDataOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RGXDestroyHWRTData_exit;
	}




RGXDestroyHWRTData_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRGXCreateRenderTarget(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXCREATERENDERTARGET *psRGXCreateRenderTargetIN,
					  PVRSRV_BRIDGE_OUT_RGXCREATERENDERTARGET *psRGXCreateRenderTargetOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_RT_CLEANUP_DATA * pssRenderTargetMemDescInt = NULL;






	psRGXCreateRenderTargetOUT->eError =
		RGXCreateRenderTarget(psConnection, OSGetDevData(psConnection),
					psRGXCreateRenderTargetIN->spsVHeapTableDevVAddr,
					&pssRenderTargetMemDescInt,
					&psRGXCreateRenderTargetOUT->ui32sRenderTargetFWDevVAddr);
	/* Exit early if bridged call fails */
	if(psRGXCreateRenderTargetOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateRenderTarget_exit;
	}






	psRGXCreateRenderTargetOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRGXCreateRenderTargetOUT->hsRenderTargetMemDesc,
							(void *) pssRenderTargetMemDescInt,
							PVRSRV_HANDLE_TYPE_RGX_FWIF_RENDERTARGET,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&RGXDestroyRenderTarget);
	if (psRGXCreateRenderTargetOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateRenderTarget_exit;
	}




RGXCreateRenderTarget_exit:


	if (psRGXCreateRenderTargetOUT->eError != PVRSRV_OK)
	{
		if (pssRenderTargetMemDescInt)
		{
			RGXDestroyRenderTarget(pssRenderTargetMemDescInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgeRGXDestroyRenderTarget(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXDESTROYRENDERTARGET *psRGXDestroyRenderTargetIN,
					  PVRSRV_BRIDGE_OUT_RGXDESTROYRENDERTARGET *psRGXDestroyRenderTargetOUT,
					 CONNECTION_DATA *psConnection)
{












	psRGXDestroyRenderTargetOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyRenderTargetIN->hsRenderTargetMemDesc,
					PVRSRV_HANDLE_TYPE_RGX_FWIF_RENDERTARGET);
	if ((psRGXDestroyRenderTargetOUT->eError != PVRSRV_OK) && (psRGXDestroyRenderTargetOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RGXDestroyRenderTarget_exit;
	}




RGXDestroyRenderTarget_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRGXCreateZSBuffer(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXCREATEZSBUFFER *psRGXCreateZSBufferIN,
					  PVRSRV_BRIDGE_OUT_RGXCREATEZSBUFFER *psRGXCreateZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hReservation = psRGXCreateZSBufferIN->hReservation;
	DEVMEMINT_RESERVATION * psReservationInt = NULL;
	IMG_HANDLE hPMR = psRGXCreateZSBufferIN->hPMR;
	PMR * psPMRInt = NULL;
	RGX_ZSBUFFER_DATA * pssZSBufferKMInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXCreateZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psReservationInt,
											hReservation,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
											IMG_TRUE);
					if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateZSBuffer_exit;
					}
				}





				{
					/* Look up the address from the handle */
					psRGXCreateZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateZSBuffer_exit;
					}
				}

	psRGXCreateZSBufferOUT->eError =
		RGXCreateZSBufferKM(psConnection, OSGetDevData(psConnection),
					psReservationInt,
					psPMRInt,
					psRGXCreateZSBufferIN->uiMapFlags,
					&pssZSBufferKMInt,
					&psRGXCreateZSBufferOUT->ui32sZSBufferFWDevVAddr);
	/* Exit early if bridged call fails */
	if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateZSBuffer_exit;
	}






	psRGXCreateZSBufferOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRGXCreateZSBufferOUT->hsZSBufferKM,
							(void *) pssZSBufferKMInt,
							PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&RGXDestroyZSBufferKM);
	if (psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateZSBuffer_exit;
	}




RGXCreateZSBuffer_exit:






				{
					/* Unreference the previously looked up handle */
						if(psReservationInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hReservation,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
						}
				}





				{
					/* Unreference the previously looked up handle */
						if(psPMRInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}

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
					  PVRSRV_BRIDGE_IN_RGXDESTROYZSBUFFER *psRGXDestroyZSBufferIN,
					  PVRSRV_BRIDGE_OUT_RGXDESTROYZSBUFFER *psRGXDestroyZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{












	psRGXDestroyZSBufferOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyZSBufferIN->hsZSBufferMemDesc,
					PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
	if ((psRGXDestroyZSBufferOUT->eError != PVRSRV_OK) && (psRGXDestroyZSBufferOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RGXDestroyZSBuffer_exit;
	}




RGXDestroyZSBuffer_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRGXPopulateZSBuffer(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXPOPULATEZSBUFFER *psRGXPopulateZSBufferIN,
					  PVRSRV_BRIDGE_OUT_RGXPOPULATEZSBUFFER *psRGXPopulateZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hsZSBufferKM = psRGXPopulateZSBufferIN->hsZSBufferKM;
	RGX_ZSBUFFER_DATA * pssZSBufferKMInt = NULL;
	RGX_POPULATION * pssPopulationInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXPopulateZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &pssZSBufferKMInt,
											hsZSBufferKM,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
											IMG_TRUE);
					if(psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXPopulateZSBuffer_exit;
					}
				}

	psRGXPopulateZSBufferOUT->eError =
		RGXPopulateZSBufferKM(
					pssZSBufferKMInt,
					&pssPopulationInt);
	/* Exit early if bridged call fails */
	if(psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXPopulateZSBuffer_exit;
	}






	psRGXPopulateZSBufferOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRGXPopulateZSBufferOUT->hsPopulation,
							(void *) pssPopulationInt,
							PVRSRV_HANDLE_TYPE_RGX_POPULATION,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&RGXUnpopulateZSBufferKM);
	if (psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXPopulateZSBuffer_exit;
	}




RGXPopulateZSBuffer_exit:






				{
					/* Unreference the previously looked up handle */
						if(pssZSBufferKMInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hsZSBufferKM,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
						}
				}

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
					  PVRSRV_BRIDGE_IN_RGXUNPOPULATEZSBUFFER *psRGXUnpopulateZSBufferIN,
					  PVRSRV_BRIDGE_OUT_RGXUNPOPULATEZSBUFFER *psRGXUnpopulateZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{












	psRGXUnpopulateZSBufferOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXUnpopulateZSBufferIN->hsPopulation,
					PVRSRV_HANDLE_TYPE_RGX_POPULATION);
	if ((psRGXUnpopulateZSBufferOUT->eError != PVRSRV_OK) && (psRGXUnpopulateZSBufferOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RGXUnpopulateZSBuffer_exit;
	}




RGXUnpopulateZSBuffer_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRGXCreateFreeList(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXCREATEFREELIST *psRGXCreateFreeListIN,
					  PVRSRV_BRIDGE_OUT_RGXCREATEFREELIST *psRGXCreateFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hsGlobalFreeList = psRGXCreateFreeListIN->hsGlobalFreeList;
	RGX_FREELIST * pssGlobalFreeListInt = NULL;
	IMG_HANDLE hsFreeListPMR = psRGXCreateFreeListIN->hsFreeListPMR;
	PMR * pssFreeListPMRInt = NULL;
	RGX_FREELIST * psCleanupCookieInt = NULL;










				if (psRGXCreateFreeListIN->hsGlobalFreeList)
				{
					/* Look up the address from the handle */
					psRGXCreateFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &pssGlobalFreeListInt,
											hsGlobalFreeList,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST,
											IMG_TRUE);
					if(psRGXCreateFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateFreeList_exit;
					}
				}





				{
					/* Look up the address from the handle */
					psRGXCreateFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &pssFreeListPMRInt,
											hsFreeListPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psRGXCreateFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateFreeList_exit;
					}
				}

	psRGXCreateFreeListOUT->eError =
		RGXCreateFreeList(psConnection, OSGetDevData(psConnection),
					psRGXCreateFreeListIN->ui32ui32MaxFLPages,
					psRGXCreateFreeListIN->ui32ui32InitFLPages,
					psRGXCreateFreeListIN->ui32ui32GrowFLPages,
					pssGlobalFreeListInt,
					psRGXCreateFreeListIN->bbFreeListCheck,
					psRGXCreateFreeListIN->spsFreeListDevVAddr,
					pssFreeListPMRInt,
					psRGXCreateFreeListIN->uiPMROffset,
					&psCleanupCookieInt);
	/* Exit early if bridged call fails */
	if(psRGXCreateFreeListOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateFreeList_exit;
	}






	psRGXCreateFreeListOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRGXCreateFreeListOUT->hCleanupCookie,
							(void *) psCleanupCookieInt,
							PVRSRV_HANDLE_TYPE_RGX_FREELIST,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&RGXDestroyFreeList);
	if (psRGXCreateFreeListOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateFreeList_exit;
	}




RGXCreateFreeList_exit:






				if (psRGXCreateFreeListIN->hsGlobalFreeList)
				{
					/* Unreference the previously looked up handle */
						if(pssGlobalFreeListInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hsGlobalFreeList,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST);
						}
				}





				{
					/* Unreference the previously looked up handle */
						if(pssFreeListPMRInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hsFreeListPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}

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
					  PVRSRV_BRIDGE_IN_RGXDESTROYFREELIST *psRGXDestroyFreeListIN,
					  PVRSRV_BRIDGE_OUT_RGXDESTROYFREELIST *psRGXDestroyFreeListOUT,
					 CONNECTION_DATA *psConnection)
{












	psRGXDestroyFreeListOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyFreeListIN->hCleanupCookie,
					PVRSRV_HANDLE_TYPE_RGX_FREELIST);
	if ((psRGXDestroyFreeListOUT->eError != PVRSRV_OK) && (psRGXDestroyFreeListOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RGXDestroyFreeList_exit;
	}




RGXDestroyFreeList_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRGXAddBlockToFreeList(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXADDBLOCKTOFREELIST *psRGXAddBlockToFreeListIN,
					  PVRSRV_BRIDGE_OUT_RGXADDBLOCKTOFREELIST *psRGXAddBlockToFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hsFreeList = psRGXAddBlockToFreeListIN->hsFreeList;
	RGX_FREELIST * pssFreeListInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXAddBlockToFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &pssFreeListInt,
											hsFreeList,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST,
											IMG_TRUE);
					if(psRGXAddBlockToFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXAddBlockToFreeList_exit;
					}
				}

	psRGXAddBlockToFreeListOUT->eError =
		RGXAddBlockToFreeListKM(
					pssFreeListInt,
					psRGXAddBlockToFreeListIN->ui3232NumPages);




RGXAddBlockToFreeList_exit:






				{
					/* Unreference the previously looked up handle */
						if(pssFreeListInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hsFreeList,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeRGXRemoveBlockFromFreeList(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXREMOVEBLOCKFROMFREELIST *psRGXRemoveBlockFromFreeListIN,
					  PVRSRV_BRIDGE_OUT_RGXREMOVEBLOCKFROMFREELIST *psRGXRemoveBlockFromFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hsFreeList = psRGXRemoveBlockFromFreeListIN->hsFreeList;
	RGX_FREELIST * pssFreeListInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXRemoveBlockFromFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &pssFreeListInt,
											hsFreeList,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST,
											IMG_TRUE);
					if(psRGXRemoveBlockFromFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXRemoveBlockFromFreeList_exit;
					}
				}

	psRGXRemoveBlockFromFreeListOUT->eError =
		RGXRemoveBlockFromFreeListKM(
					pssFreeListInt);




RGXRemoveBlockFromFreeList_exit:






				{
					/* Unreference the previously looked up handle */
						if(pssFreeListInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hsFreeList,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeRGXCreateRenderContext(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXCREATERENDERCONTEXT *psRGXCreateRenderContextIN,
					  PVRSRV_BRIDGE_OUT_RGXCREATERENDERCONTEXT *psRGXCreateRenderContextOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_BYTE *psFrameworkCmdInt = NULL;
	IMG_HANDLE hPrivData = psRGXCreateRenderContextIN->hPrivData;
	IMG_HANDLE hPrivDataInt = NULL;
	RGX_SERVER_RENDER_CONTEXT * psRenderContextInt = NULL;




	if (psRGXCreateRenderContextIN->ui32FrameworkCmdize != 0)
	{
		psFrameworkCmdInt = OSAllocZMemNoStats(psRGXCreateRenderContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE));
		if (!psFrameworkCmdInt)
		{
			psRGXCreateRenderContextOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXCreateRenderContext_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXCreateRenderContextIN->psFrameworkCmd, psRGXCreateRenderContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, psFrameworkCmdInt, psRGXCreateRenderContextIN->psFrameworkCmd,
				psRGXCreateRenderContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXCreateRenderContextOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXCreateRenderContext_exit;
			}






				{
					/* Look up the address from the handle */
					psRGXCreateRenderContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &hPrivDataInt,
											hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
											IMG_TRUE);
					if(psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateRenderContext_exit;
					}
				}

	psRGXCreateRenderContextOUT->eError =
		PVRSRVRGXCreateRenderContextKM(psConnection, OSGetDevData(psConnection),
					psRGXCreateRenderContextIN->ui32Priority,
					psRGXCreateRenderContextIN->sMCUFenceAddr,
					psRGXCreateRenderContextIN->sVDMCallStackAddr,
					psRGXCreateRenderContextIN->ui32FrameworkCmdize,
					psFrameworkCmdInt,
					hPrivDataInt,
					&psRenderContextInt);
	/* Exit early if bridged call fails */
	if(psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateRenderContext_exit;
	}






	psRGXCreateRenderContextOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRGXCreateRenderContextOUT->hRenderContext,
							(void *) psRenderContextInt,
							PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PVRSRVRGXDestroyRenderContextKM);
	if (psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateRenderContext_exit;
	}




RGXCreateRenderContext_exit:






				{
					/* Unreference the previously looked up handle */
						if(hPrivDataInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
						}
				}

	if (psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
	{
		if (psRenderContextInt)
		{
			PVRSRVRGXDestroyRenderContextKM(psRenderContextInt);
		}
	}

	if (psFrameworkCmdInt)
		OSFreeMemNoStats(psFrameworkCmdInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeRGXDestroyRenderContext(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXDESTROYRENDERCONTEXT *psRGXDestroyRenderContextIN,
					  PVRSRV_BRIDGE_OUT_RGXDESTROYRENDERCONTEXT *psRGXDestroyRenderContextOUT,
					 CONNECTION_DATA *psConnection)
{












	psRGXDestroyRenderContextOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyRenderContextIN->hCleanupCookie,
					PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
	if ((psRGXDestroyRenderContextOUT->eError != PVRSRV_OK) && (psRGXDestroyRenderContextOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RGXDestroyRenderContext_exit;
	}




RGXDestroyRenderContext_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRGXKickTA3D(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXKICKTA3D *psRGXKickTA3DIN,
					  PVRSRV_BRIDGE_OUT_RGXKICKTA3D *psRGXKickTA3DOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hRenderContext = psRGXKickTA3DIN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT * psRenderContextInt = NULL;
	SYNC_PRIMITIVE_BLOCK * *psClientTAFenceSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClientTAFenceSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32ClientTAFenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32ClientTAFenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK * *psClientTAUpdateSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClientTAUpdateSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32ClientTAUpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32ClientTAUpdateValueInt = NULL;
	IMG_UINT32 *ui32ServerTASyncFlagsInt = NULL;
	SERVER_SYNC_PRIMITIVE * *psServerTASyncsInt = NULL;
	IMG_HANDLE *hServerTASyncsInt2 = NULL;
	SYNC_PRIMITIVE_BLOCK * *psClient3DFenceSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClient3DFenceSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32Client3DFenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32Client3DFenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK * *psClient3DUpdateSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClient3DUpdateSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32Client3DUpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32Client3DUpdateValueInt = NULL;
	IMG_UINT32 *ui32Server3DSyncFlagsInt = NULL;
	SERVER_SYNC_PRIMITIVE * *psServer3DSyncsInt = NULL;
	IMG_HANDLE *hServer3DSyncsInt2 = NULL;
	IMG_HANDLE hPRFenceUFOSyncPrimBlock = psRGXKickTA3DIN->hPRFenceUFOSyncPrimBlock;
	SYNC_PRIMITIVE_BLOCK * psPRFenceUFOSyncPrimBlockInt = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_BYTE *psTACmdInt = NULL;
	IMG_BYTE *ps3DPRCmdInt = NULL;
	IMG_BYTE *ps3DCmdInt = NULL;
	IMG_HANDLE hRTDataCleanup = psRGXKickTA3DIN->hRTDataCleanup;
	RGX_RTDATA_CLEANUP_DATA * psRTDataCleanupInt = NULL;
	IMG_HANDLE hZBuffer = psRGXKickTA3DIN->hZBuffer;
	RGX_ZSBUFFER_DATA * psZBufferInt = NULL;
	IMG_HANDLE hSBuffer = psRGXKickTA3DIN->hSBuffer;
	RGX_ZSBUFFER_DATA * psSBufferInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR * *psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;




	if (psRGXKickTA3DIN->ui32ClientTAFenceCount != 0)
	{
		psClientTAFenceSyncPrimBlockInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(SYNC_PRIMITIVE_BLOCK *));
		if (!psClientTAFenceSyncPrimBlockInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
		hClientTAFenceSyncPrimBlockInt2 = OSAllocMemNoStats(psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_HANDLE));
		if (!hClientTAFenceSyncPrimBlockInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->phClientTAFenceSyncPrimBlock, psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hClientTAFenceSyncPrimBlockInt2, psRGXKickTA3DIN->phClientTAFenceSyncPrimBlock,
				psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ClientTAFenceCount != 0)
	{
		ui32ClientTAFenceSyncOffsetInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32));
		if (!ui32ClientTAFenceSyncOffsetInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32ClientTAFenceSyncOffset, psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientTAFenceSyncOffsetInt, psRGXKickTA3DIN->pui32ClientTAFenceSyncOffset,
				psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ClientTAFenceCount != 0)
	{
		ui32ClientTAFenceValueInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32));
		if (!ui32ClientTAFenceValueInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32ClientTAFenceValue, psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientTAFenceValueInt, psRGXKickTA3DIN->pui32ClientTAFenceValue,
				psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount != 0)
	{
		psClientTAUpdateSyncPrimBlockInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(SYNC_PRIMITIVE_BLOCK *));
		if (!psClientTAUpdateSyncPrimBlockInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
		hClientTAUpdateSyncPrimBlockInt2 = OSAllocMemNoStats(psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_HANDLE));
		if (!hClientTAUpdateSyncPrimBlockInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->phClientTAUpdateSyncPrimBlock, psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hClientTAUpdateSyncPrimBlockInt2, psRGXKickTA3DIN->phClientTAUpdateSyncPrimBlock,
				psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount != 0)
	{
		ui32ClientTAUpdateSyncOffsetInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32));
		if (!ui32ClientTAUpdateSyncOffsetInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32ClientTAUpdateSyncOffset, psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientTAUpdateSyncOffsetInt, psRGXKickTA3DIN->pui32ClientTAUpdateSyncOffset,
				psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount != 0)
	{
		ui32ClientTAUpdateValueInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32));
		if (!ui32ClientTAUpdateValueInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32ClientTAUpdateValue, psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientTAUpdateValueInt, psRGXKickTA3DIN->pui32ClientTAUpdateValue,
				psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ServerTASyncPrims != 0)
	{
		ui32ServerTASyncFlagsInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32));
		if (!ui32ServerTASyncFlagsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32ServerTASyncFlags, psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ServerTASyncFlagsInt, psRGXKickTA3DIN->pui32ServerTASyncFlags,
				psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ServerTASyncPrims != 0)
	{
		psServerTASyncsInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psServerTASyncsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
		hServerTASyncsInt2 = OSAllocMemNoStats(psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE));
		if (!hServerTASyncsInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->phServerTASyncs, psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hServerTASyncsInt2, psRGXKickTA3DIN->phServerTASyncs,
				psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DFenceCount != 0)
	{
		psClient3DFenceSyncPrimBlockInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(SYNC_PRIMITIVE_BLOCK *));
		if (!psClient3DFenceSyncPrimBlockInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
		hClient3DFenceSyncPrimBlockInt2 = OSAllocMemNoStats(psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_HANDLE));
		if (!hClient3DFenceSyncPrimBlockInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->phClient3DFenceSyncPrimBlock, psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hClient3DFenceSyncPrimBlockInt2, psRGXKickTA3DIN->phClient3DFenceSyncPrimBlock,
				psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DFenceCount != 0)
	{
		ui32Client3DFenceSyncOffsetInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32));
		if (!ui32Client3DFenceSyncOffsetInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32Client3DFenceSyncOffset, psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32Client3DFenceSyncOffsetInt, psRGXKickTA3DIN->pui32Client3DFenceSyncOffset,
				psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DFenceCount != 0)
	{
		ui32Client3DFenceValueInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32));
		if (!ui32Client3DFenceValueInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32Client3DFenceValue, psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32Client3DFenceValueInt, psRGXKickTA3DIN->pui32Client3DFenceValue,
				psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount != 0)
	{
		psClient3DUpdateSyncPrimBlockInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(SYNC_PRIMITIVE_BLOCK *));
		if (!psClient3DUpdateSyncPrimBlockInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
		hClient3DUpdateSyncPrimBlockInt2 = OSAllocMemNoStats(psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_HANDLE));
		if (!hClient3DUpdateSyncPrimBlockInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->phClient3DUpdateSyncPrimBlock, psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hClient3DUpdateSyncPrimBlockInt2, psRGXKickTA3DIN->phClient3DUpdateSyncPrimBlock,
				psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount != 0)
	{
		ui32Client3DUpdateSyncOffsetInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32));
		if (!ui32Client3DUpdateSyncOffsetInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32Client3DUpdateSyncOffset, psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32Client3DUpdateSyncOffsetInt, psRGXKickTA3DIN->pui32Client3DUpdateSyncOffset,
				psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount != 0)
	{
		ui32Client3DUpdateValueInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32));
		if (!ui32Client3DUpdateValueInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32Client3DUpdateValue, psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32Client3DUpdateValueInt, psRGXKickTA3DIN->pui32Client3DUpdateValue,
				psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Server3DSyncPrims != 0)
	{
		ui32Server3DSyncFlagsInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32));
		if (!ui32Server3DSyncFlagsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32Server3DSyncFlags, psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32Server3DSyncFlagsInt, psRGXKickTA3DIN->pui32Server3DSyncFlags,
				psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Server3DSyncPrims != 0)
	{
		psServer3DSyncsInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psServer3DSyncsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
		hServer3DSyncsInt2 = OSAllocMemNoStats(psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE));
		if (!hServer3DSyncsInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->phServer3DSyncs, psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hServer3DSyncsInt2, psRGXKickTA3DIN->phServer3DSyncs,
				psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	
	{
		uiUpdateFenceNameInt = OSAllocZMemNoStats(32 * sizeof(IMG_CHAR));
		if (!uiUpdateFenceNameInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->puiUpdateFenceName, 32 * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiUpdateFenceNameInt, psRGXKickTA3DIN->puiUpdateFenceName,
				32 * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32TACmdSize != 0)
	{
		psTACmdInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE));
		if (!psTACmdInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->psTACmd, psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, psTACmdInt, psRGXKickTA3DIN->psTACmd,
				psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui323DPRCmdSize != 0)
	{
		ps3DPRCmdInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE));
		if (!ps3DPRCmdInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->ps3DPRCmd, psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, ps3DPRCmdInt, psRGXKickTA3DIN->ps3DPRCmd,
				psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui323DCmdSize != 0)
	{
		ps3DCmdInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE));
		if (!ps3DCmdInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->ps3DCmd, psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, ps3DCmdInt, psRGXKickTA3DIN->ps3DCmd,
				psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_UINT32));
		if (!ui32SyncPMRFlagsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->pui32SyncPMRFlags, psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32SyncPMRFlagsInt, psRGXKickTA3DIN->pui32SyncPMRFlags,
				psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt = OSAllocZMemNoStats(psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(PMR *));
		if (!psSyncPMRsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
		hSyncPMRsInt2 = OSAllocMemNoStats(psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_HANDLE));
		if (!hSyncPMRsInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXKickTA3DIN->phSyncPMRs, psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hSyncPMRsInt2, psRGXKickTA3DIN->phSyncPMRs,
				psRGXKickTA3DIN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}






				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psRenderContextInt,
											hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32ClientTAFenceCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psClientTAFenceSyncPrimBlockInt[i],
											hClientTAFenceSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32ClientTAUpdateCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psClientTAUpdateSyncPrimBlockInt[i],
											hClientTAUpdateSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32ServerTASyncPrims;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServerTASyncsInt[i],
											hServerTASyncsInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32Client3DFenceCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psClient3DFenceSyncPrimBlockInt[i],
											hClient3DFenceSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32Client3DUpdateCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psClient3DUpdateSyncPrimBlockInt[i],
											hClient3DUpdateSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32Server3DSyncPrims;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServer3DSyncsInt[i],
											hServer3DSyncsInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}





				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPRFenceUFOSyncPrimBlockInt,
											hPRFenceUFOSyncPrimBlock,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}





				if (psRGXKickTA3DIN->hRTDataCleanup)
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psRTDataCleanupInt,
											hRTDataCleanup,
											PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}





				if (psRGXKickTA3DIN->hZBuffer)
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psZBufferInt,
											hZBuffer,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}





				if (psRGXKickTA3DIN->hSBuffer)
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSBufferInt,
											hSBuffer,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32SyncPMRCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncPMRsInt[i],
											hSyncPMRsInt2[i],
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}

	psRGXKickTA3DOUT->eError =
		PVRSRVRGXKickTA3DKM(
					psRenderContextInt,
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
					psRGXKickTA3DIN->i32CheckFenceFD,
					psRGXKickTA3DIN->i32UpdateTimelineFD,
					&psRGXKickTA3DOUT->i32UpdateFenceFD,
					uiUpdateFenceNameInt,
					psRGXKickTA3DIN->ui32TACmdSize,
					psTACmdInt,
					psRGXKickTA3DIN->ui323DPRCmdSize,
					ps3DPRCmdInt,
					psRGXKickTA3DIN->ui323DCmdSize,
					ps3DCmdInt,
					psRGXKickTA3DIN->ui32ExtJobRef,
					psRGXKickTA3DIN->bbLastTAInScene,
					psRGXKickTA3DIN->bbKickTA,
					psRGXKickTA3DIN->bbKickPR,
					psRGXKickTA3DIN->bbKick3D,
					psRGXKickTA3DIN->bbAbort,
					psRGXKickTA3DIN->bbPDumpContinuous,
					psRTDataCleanupInt,
					psZBufferInt,
					psSBufferInt,
					psRGXKickTA3DIN->bbCommitRefCountsTA,
					psRGXKickTA3DIN->bbCommitRefCounts3D,
					&psRGXKickTA3DOUT->bbCommittedRefCountsTA,
					&psRGXKickTA3DOUT->bbCommittedRefCounts3D,
					psRGXKickTA3DIN->ui32SyncPMRCount,
					ui32SyncPMRFlagsInt,
					psSyncPMRsInt,
					psRGXKickTA3DIN->ui32RenderTargetSize,
					psRGXKickTA3DIN->ui32NumberOfDrawCalls,
					psRGXKickTA3DIN->ui32NumberOfIndices,
					psRGXKickTA3DIN->ui32NumberOfMRTs,
					psRGXKickTA3DIN->ui64Deadline);




RGXKickTA3D_exit:






				{
					/* Unreference the previously looked up handle */
						if(psRenderContextInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
						}
				}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32ClientTAFenceCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psClientTAFenceSyncPrimBlockInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hClientTAFenceSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32ClientTAUpdateCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psClientTAUpdateSyncPrimBlockInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hClientTAUpdateSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32ServerTASyncPrims;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psServerTASyncsInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hServerTASyncsInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
						}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32Client3DFenceCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psClient3DFenceSyncPrimBlockInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hClient3DFenceSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32Client3DUpdateCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psClient3DUpdateSyncPrimBlockInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hClient3DUpdateSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32Server3DSyncPrims;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psServer3DSyncsInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hServer3DSyncsInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
						}
				}
		}
	}





				{
					/* Unreference the previously looked up handle */
						if(psPRFenceUFOSyncPrimBlockInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPRFenceUFOSyncPrimBlock,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}





				if (psRGXKickTA3DIN->hRTDataCleanup)
				{
					/* Unreference the previously looked up handle */
						if(psRTDataCleanupInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hRTDataCleanup,
											PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP);
						}
				}





				if (psRGXKickTA3DIN->hZBuffer)
				{
					/* Unreference the previously looked up handle */
						if(psZBufferInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hZBuffer,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
						}
				}





				if (psRGXKickTA3DIN->hSBuffer)
				{
					/* Unreference the previously looked up handle */
						if(psSBufferInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSBuffer,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
						}
				}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32SyncPMRCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psSyncPMRsInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncPMRsInt2[i],
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}
		}
	}

	if (psClientTAFenceSyncPrimBlockInt)
		OSFreeMemNoStats(psClientTAFenceSyncPrimBlockInt);
	if (hClientTAFenceSyncPrimBlockInt2)
		OSFreeMemNoStats(hClientTAFenceSyncPrimBlockInt2);
	if (ui32ClientTAFenceSyncOffsetInt)
		OSFreeMemNoStats(ui32ClientTAFenceSyncOffsetInt);
	if (ui32ClientTAFenceValueInt)
		OSFreeMemNoStats(ui32ClientTAFenceValueInt);
	if (psClientTAUpdateSyncPrimBlockInt)
		OSFreeMemNoStats(psClientTAUpdateSyncPrimBlockInt);
	if (hClientTAUpdateSyncPrimBlockInt2)
		OSFreeMemNoStats(hClientTAUpdateSyncPrimBlockInt2);
	if (ui32ClientTAUpdateSyncOffsetInt)
		OSFreeMemNoStats(ui32ClientTAUpdateSyncOffsetInt);
	if (ui32ClientTAUpdateValueInt)
		OSFreeMemNoStats(ui32ClientTAUpdateValueInt);
	if (ui32ServerTASyncFlagsInt)
		OSFreeMemNoStats(ui32ServerTASyncFlagsInt);
	if (psServerTASyncsInt)
		OSFreeMemNoStats(psServerTASyncsInt);
	if (hServerTASyncsInt2)
		OSFreeMemNoStats(hServerTASyncsInt2);
	if (psClient3DFenceSyncPrimBlockInt)
		OSFreeMemNoStats(psClient3DFenceSyncPrimBlockInt);
	if (hClient3DFenceSyncPrimBlockInt2)
		OSFreeMemNoStats(hClient3DFenceSyncPrimBlockInt2);
	if (ui32Client3DFenceSyncOffsetInt)
		OSFreeMemNoStats(ui32Client3DFenceSyncOffsetInt);
	if (ui32Client3DFenceValueInt)
		OSFreeMemNoStats(ui32Client3DFenceValueInt);
	if (psClient3DUpdateSyncPrimBlockInt)
		OSFreeMemNoStats(psClient3DUpdateSyncPrimBlockInt);
	if (hClient3DUpdateSyncPrimBlockInt2)
		OSFreeMemNoStats(hClient3DUpdateSyncPrimBlockInt2);
	if (ui32Client3DUpdateSyncOffsetInt)
		OSFreeMemNoStats(ui32Client3DUpdateSyncOffsetInt);
	if (ui32Client3DUpdateValueInt)
		OSFreeMemNoStats(ui32Client3DUpdateValueInt);
	if (ui32Server3DSyncFlagsInt)
		OSFreeMemNoStats(ui32Server3DSyncFlagsInt);
	if (psServer3DSyncsInt)
		OSFreeMemNoStats(psServer3DSyncsInt);
	if (hServer3DSyncsInt2)
		OSFreeMemNoStats(hServer3DSyncsInt2);
	if (uiUpdateFenceNameInt)
		OSFreeMemNoStats(uiUpdateFenceNameInt);
	if (psTACmdInt)
		OSFreeMemNoStats(psTACmdInt);
	if (ps3DPRCmdInt)
		OSFreeMemNoStats(ps3DPRCmdInt);
	if (ps3DCmdInt)
		OSFreeMemNoStats(ps3DCmdInt);
	if (ui32SyncPMRFlagsInt)
		OSFreeMemNoStats(ui32SyncPMRFlagsInt);
	if (psSyncPMRsInt)
		OSFreeMemNoStats(psSyncPMRsInt);
	if (hSyncPMRsInt2)
		OSFreeMemNoStats(hSyncPMRsInt2);

	return 0;
}


static IMG_INT
PVRSRVBridgeRGXSetRenderContextPriority(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXSETRENDERCONTEXTPRIORITY *psRGXSetRenderContextPriorityIN,
					  PVRSRV_BRIDGE_OUT_RGXSETRENDERCONTEXTPRIORITY *psRGXSetRenderContextPriorityOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hRenderContext = psRGXSetRenderContextPriorityIN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT * psRenderContextInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXSetRenderContextPriorityOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psRenderContextInt,
											hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
											IMG_TRUE);
					if(psRGXSetRenderContextPriorityOUT->eError != PVRSRV_OK)
					{
						goto RGXSetRenderContextPriority_exit;
					}
				}

	psRGXSetRenderContextPriorityOUT->eError =
		PVRSRVRGXSetRenderContextPriorityKM(psConnection, OSGetDevData(psConnection),
					psRenderContextInt,
					psRGXSetRenderContextPriorityIN->ui32Priority);




RGXSetRenderContextPriority_exit:






				{
					/* Unreference the previously looked up handle */
						if(psRenderContextInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeRGXGetLastRenderContextResetReason(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXGETLASTRENDERCONTEXTRESETREASON *psRGXGetLastRenderContextResetReasonIN,
					  PVRSRV_BRIDGE_OUT_RGXGETLASTRENDERCONTEXTRESETREASON *psRGXGetLastRenderContextResetReasonOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hRenderContext = psRGXGetLastRenderContextResetReasonIN->hRenderContext;
	RGX_SERVER_RENDER_CONTEXT * psRenderContextInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXGetLastRenderContextResetReasonOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psRenderContextInt,
											hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
											IMG_TRUE);
					if(psRGXGetLastRenderContextResetReasonOUT->eError != PVRSRV_OK)
					{
						goto RGXGetLastRenderContextResetReason_exit;
					}
				}

	psRGXGetLastRenderContextResetReasonOUT->eError =
		PVRSRVRGXGetLastRenderContextResetReasonKM(
					psRenderContextInt,
					&psRGXGetLastRenderContextResetReasonOUT->ui32LastResetReason,
					&psRGXGetLastRenderContextResetReasonOUT->ui32LastResetJobRef);




RGXGetLastRenderContextResetReason_exit:






				{
					/* Unreference the previously looked up handle */
						if(psRenderContextInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeRGXGetPartialRenderCount(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXGETPARTIALRENDERCOUNT *psRGXGetPartialRenderCountIN,
					  PVRSRV_BRIDGE_OUT_RGXGETPARTIALRENDERCOUNT *psRGXGetPartialRenderCountOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hHWRTDataMemDesc = psRGXGetPartialRenderCountIN->hHWRTDataMemDesc;
	DEVMEM_MEMDESC * psHWRTDataMemDescInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXGetPartialRenderCountOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psHWRTDataMemDescInt,
											hHWRTDataMemDesc,
											PVRSRV_HANDLE_TYPE_RGX_FW_MEMDESC,
											IMG_TRUE);
					if(psRGXGetPartialRenderCountOUT->eError != PVRSRV_OK)
					{
						goto RGXGetPartialRenderCount_exit;
					}
				}

	psRGXGetPartialRenderCountOUT->eError =
		PVRSRVRGXGetPartialRenderCountKM(
					psHWRTDataMemDescInt,
					&psRGXGetPartialRenderCountOUT->ui32NumPartialRenders);




RGXGetPartialRenderCount_exit:






				{
					/* Unreference the previously looked up handle */
						if(psHWRTDataMemDescInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hHWRTDataMemDesc,
											PVRSRV_HANDLE_TYPE_RGX_FW_MEMDESC);
						}
				}


	return 0;
}




/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */

static IMG_BOOL bUseLock = IMG_TRUE;

PVRSRV_ERROR InitRGXTA3DBridge(void);
PVRSRV_ERROR DeinitRGXTA3DBridge(void);

/*
 * Register all RGXTA3D functions with services
 */
PVRSRV_ERROR InitRGXTA3DBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATA, PVRSRVBridgeRGXCreateHWRTData,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATA, PVRSRVBridgeRGXDestroyHWRTData,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERTARGET, PVRSRVBridgeRGXCreateRenderTarget,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERTARGET, PVRSRVBridgeRGXDestroyRenderTarget,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXCREATEZSBUFFER, PVRSRVBridgeRGXCreateZSBuffer,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYZSBUFFER, PVRSRVBridgeRGXDestroyZSBuffer,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXPOPULATEZSBUFFER, PVRSRVBridgeRGXPopulateZSBuffer,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXUNPOPULATEZSBUFFER, PVRSRVBridgeRGXUnpopulateZSBuffer,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST, PVRSRVBridgeRGXCreateFreeList,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST, PVRSRVBridgeRGXDestroyFreeList,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXADDBLOCKTOFREELIST, PVRSRVBridgeRGXAddBlockToFreeList,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXREMOVEBLOCKFROMFREELIST, PVRSRVBridgeRGXRemoveBlockFromFreeList,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT, PVRSRVBridgeRGXCreateRenderContext,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT, PVRSRVBridgeRGXDestroyRenderContext,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXKICKTA3D, PVRSRVBridgeRGXKickTA3D,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXSETRENDERCONTEXTPRIORITY, PVRSRVBridgeRGXSetRenderContextPriority,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXGETLASTRENDERCONTEXTRESETREASON, PVRSRVBridgeRGXGetLastRenderContextResetReason,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D, PVRSRV_BRIDGE_RGXTA3D_RGXGETPARTIALRENDERCOUNT, PVRSRVBridgeRGXGetPartialRenderCount,
					NULL, bUseLock);


	return PVRSRV_OK;
}

/*
 * Unregister all rgxta3d functions with services
 */
PVRSRV_ERROR DeinitRGXTA3DBridge(void)
{
	return PVRSRV_OK;
}
