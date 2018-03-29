/*************************************************************************/ /*!
@File
@Title          Server bridge for mm
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for mm
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

#include "devicemem.h"
#include "devicemem_server.h"
#include "pmr.h"
#include "devicemem_heapcfg.h"
#include "physmem.h"


#include "common_mm_bridge.h"

#include "allocmem.h"
#include "pvr_debug.h"
#include "connection_server.h"
#include "pvr_bridge.h"
#include "rgx_bridge.h"
#include "srvcore.h"
#include "handle.h"

#include <linux/slab.h>



static PVRSRV_ERROR ReleasePMRExport(void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(pvData);

	return PVRSRV_OK;
}


/* ***************************************************************************
 * Server-side bridge entry points
 */
 
static IMG_INT
PVRSRVBridgePMRExportPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMREXPORTPMR *psPMRExportPMRIN,
					  PVRSRV_BRIDGE_OUT_PMREXPORTPMR *psPMRExportPMROUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMR = psPMRExportPMRIN->hPMR;
	PMR * psPMRInt = NULL;
	PMR_EXPORT * psPMRExportInt = NULL;
	IMG_HANDLE hPMRExportInt = NULL;










				{
					/* Look up the address from the handle */
					psPMRExportPMROUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psPMRExportPMROUT->eError != PVRSRV_OK)
					{
						goto PMRExportPMR_exit;
					}
				}

	psPMRExportPMROUT->eError =
		PMRExportPMR(
					psPMRInt,
					&psPMRExportInt,
					&psPMRExportPMROUT->ui64Size,
					&psPMRExportPMROUT->ui32Log2Contig,
					&psPMRExportPMROUT->ui64Password);
	/* Exit early if bridged call fails */
	if(psPMRExportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRExportPMR_exit;
	}


	/*
	 * For cases where we need a cross process handle we actually allocate two.
	 * 
	 * The first one is a connection specific handle and it gets given the real
	 * release function. This handle does *NOT* get returned to the caller. It's
	 * purpose is to release any leaked resources when we either have a bad or
	 * abnormally terminated client. If we didn't do this then the resource
	 * wouldn't be freed until driver unload. If the resource is freed normally,
	 * this handle can be looked up via the cross process handle and then
	 * released accordingly.
	 * 
	 * The second one is a cross process handle and it gets given a noop release
	 * function. This handle does get returned to the caller.
	 */




	psPMRExportPMROUT->eError = PVRSRVAllocHandle(psConnection->psProcessHandleBase->psHandleBase,

							&hPMRExportInt,
							(void *) psPMRExportInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PMRUnexportPMR);
	if (psPMRExportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRExportPMR_exit;
	}

	psPMRExportPMROUT->eError = PVRSRVAllocHandle(KERNEL_HANDLE_BASE,
							&psPMRExportPMROUT->hPMRExport,
							(void *) psPMRExportInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
							(PFN_HANDLE_RELEASE)&ReleasePMRExport);
	if (psPMRExportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRExportPMR_exit;
	}



PMRExportPMR_exit:






				{
					/* Unreference the previously looked up handle */
						if(psPMRInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}

	if (psPMRExportPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRExportPMROUT->hPMRExport)
		{


			PVRSRV_ERROR eError = PVRSRVReleaseHandle(KERNEL_HANDLE_BASE,
						(IMG_HANDLE) psPMRExportPMROUT->hPMRExport,
						PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);

			/* Releasing the handle should free/destroy/release the resource. This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

		}

		if (hPMRExportInt)
		{
			PVRSRV_ERROR eError = PVRSRVReleaseHandle(psConnection->psProcessHandleBase->psHandleBase,
						hPMRExportInt,
						PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);

			/* Releasing the handle should free/destroy/release the resource. This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

			/* Avoid freeing/destroying/releasing the resource a second time below */
			psPMRExportInt = NULL;
		}

		if (psPMRExportInt)
		{
			PMRUnexportPMR(psPMRExportInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgePMRUnexportPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMRUNEXPORTPMR *psPMRUnexportPMRIN,
					  PVRSRV_BRIDGE_OUT_PMRUNEXPORTPMR *psPMRUnexportPMROUT,
					 CONNECTION_DATA *psConnection)
{
	PMR_EXPORT * psPMRExportInt = NULL;
	IMG_HANDLE hPMRExportInt = NULL;

	PVR_UNREFERENCED_PARAMETER(psConnection);








	psPMRUnexportPMROUT->eError =
		PVRSRVLookupHandle(KERNEL_HANDLE_BASE,
					(void **) &psPMRExportInt,
					(IMG_HANDLE) psPMRUnexportPMRIN->hPMRExport,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT,
					IMG_FALSE);
	PVR_ASSERT(psPMRUnexportPMROUT->eError == PVRSRV_OK);

	/*
	 * Find the connection specific handle that represents the same data
	 * as the cross process handle as releasing it will actually call the
	 * data's real release function (see the function where the cross
	 * process handle is allocated for more details).
	 */
	psPMRUnexportPMROUT->eError =
		PVRSRVFindHandle(psConnection->psProcessHandleBase->psHandleBase,
					&hPMRExportInt,
					psPMRExportInt,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
	PVR_ASSERT(psPMRUnexportPMROUT->eError == PVRSRV_OK);

	psPMRUnexportPMROUT->eError =
		PVRSRVReleaseHandle(psConnection->psProcessHandleBase->psHandleBase,
					hPMRExportInt,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
	PVR_ASSERT((psPMRUnexportPMROUT->eError == PVRSRV_OK) || (psPMRUnexportPMROUT->eError == PVRSRV_ERROR_RETRY));





	psPMRUnexportPMROUT->eError =
		PVRSRVReleaseHandle(KERNEL_HANDLE_BASE,
					(IMG_HANDLE) psPMRUnexportPMRIN->hPMRExport,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
	if ((psPMRUnexportPMROUT->eError != PVRSRV_OK) && (psPMRUnexportPMROUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto PMRUnexportPMR_exit;
	}




PMRUnexportPMR_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgePMRGetUID(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMRGETUID *psPMRGetUIDIN,
					  PVRSRV_BRIDGE_OUT_PMRGETUID *psPMRGetUIDOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMR = psPMRGetUIDIN->hPMR;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psPMRGetUIDOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psPMRGetUIDOUT->eError != PVRSRV_OK)
					{
						goto PMRGetUID_exit;
					}
				}

	psPMRGetUIDOUT->eError =
		PMRGetUID(
					psPMRInt,
					&psPMRGetUIDOUT->ui64UID);




PMRGetUID_exit:






				{
					/* Unreference the previously looked up handle */
						if(psPMRInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgePMRMakeLocalImportHandle(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMRMAKELOCALIMPORTHANDLE *psPMRMakeLocalImportHandleIN,
					  PVRSRV_BRIDGE_OUT_PMRMAKELOCALIMPORTHANDLE *psPMRMakeLocalImportHandleOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hBuffer = psPMRMakeLocalImportHandleIN->hBuffer;
	PMR * psBufferInt = NULL;
	PMR * psExtMemInt = NULL;










				{
					/* Look up the address from the handle */
					psPMRMakeLocalImportHandleOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psBufferInt,
											hBuffer,
											PVRSRV_HANDLE_TYPE_PMR_LOCAL_EXPORT_HANDLE,
											IMG_TRUE);
					if(psPMRMakeLocalImportHandleOUT->eError != PVRSRV_OK)
					{
						goto PMRMakeLocalImportHandle_exit;
					}
				}

	psPMRMakeLocalImportHandleOUT->eError =
		PMRMakeLocalImportHandle(
					psBufferInt,
					&psExtMemInt);
	/* Exit early if bridged call fails */
	if(psPMRMakeLocalImportHandleOUT->eError != PVRSRV_OK)
	{
		goto PMRMakeLocalImportHandle_exit;
	}






	psPMRMakeLocalImportHandleOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psPMRMakeLocalImportHandleOUT->hExtMem,
							(void *) psExtMemInt,
							PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PMRUnmakeLocalImportHandle);
	if (psPMRMakeLocalImportHandleOUT->eError != PVRSRV_OK)
	{
		goto PMRMakeLocalImportHandle_exit;
	}




PMRMakeLocalImportHandle_exit:






				{
					/* Unreference the previously looked up handle */
						if(psBufferInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hBuffer,
											PVRSRV_HANDLE_TYPE_PMR_LOCAL_EXPORT_HANDLE);
						}
				}

	if (psPMRMakeLocalImportHandleOUT->eError != PVRSRV_OK)
	{
		if (psExtMemInt)
		{
			PMRUnmakeLocalImportHandle(psExtMemInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgePMRUnmakeLocalImportHandle(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMRUNMAKELOCALIMPORTHANDLE *psPMRUnmakeLocalImportHandleIN,
					  PVRSRV_BRIDGE_OUT_PMRUNMAKELOCALIMPORTHANDLE *psPMRUnmakeLocalImportHandleOUT,
					 CONNECTION_DATA *psConnection)
{












	psPMRUnmakeLocalImportHandleOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psPMRUnmakeLocalImportHandleIN->hExtMem,
					PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT);
	if ((psPMRUnmakeLocalImportHandleOUT->eError != PVRSRV_OK) && (psPMRUnmakeLocalImportHandleOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto PMRUnmakeLocalImportHandle_exit;
	}




PMRUnmakeLocalImportHandle_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgePMRImportPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMRIMPORTPMR *psPMRImportPMRIN,
					  PVRSRV_BRIDGE_OUT_PMRIMPORTPMR *psPMRImportPMROUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMRExport = psPMRImportPMRIN->hPMRExport;
	PMR_EXPORT * psPMRExportInt = NULL;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psPMRImportPMROUT->eError =
						PVRSRVLookupHandle(KERNEL_HANDLE_BASE,
											(void **) &psPMRExportInt,
											hPMRExport,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT,
											IMG_TRUE);
					if(psPMRImportPMROUT->eError != PVRSRV_OK)
					{
						goto PMRImportPMR_exit;
					}
				}

	psPMRImportPMROUT->eError =
		PMRImportPMR(psConnection, OSGetDevData(psConnection),
					psPMRExportInt,
					psPMRImportPMRIN->ui64uiPassword,
					psPMRImportPMRIN->ui64uiSize,
					psPMRImportPMRIN->ui32uiLog2Contig,
					&psPMRInt);
	/* Exit early if bridged call fails */
	if(psPMRImportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRImportPMR_exit;
	}






	psPMRImportPMROUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psPMRImportPMROUT->hPMR,
							(void *) psPMRInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PMRUnrefPMR);
	if (psPMRImportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRImportPMR_exit;
	}




PMRImportPMR_exit:






				{
					/* Unreference the previously looked up handle */
						if(psPMRExportInt)
						{
							PVRSRVReleaseHandle(KERNEL_HANDLE_BASE,
											hPMRExport,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR_EXPORT);
						}
				}

	if (psPMRImportPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRInt)
		{
			PMRUnrefPMR(psPMRInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgePMRLocalImportPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMRLOCALIMPORTPMR *psPMRLocalImportPMRIN,
					  PVRSRV_BRIDGE_OUT_PMRLOCALIMPORTPMR *psPMRLocalImportPMROUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hExtHandle = psPMRLocalImportPMRIN->hExtHandle;
	PMR * psExtHandleInt = NULL;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psPMRLocalImportPMROUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psExtHandleInt,
											hExtHandle,
											PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT,
											IMG_TRUE);
					if(psPMRLocalImportPMROUT->eError != PVRSRV_OK)
					{
						goto PMRLocalImportPMR_exit;
					}
				}

	psPMRLocalImportPMROUT->eError =
		PMRLocalImportPMR(
					psExtHandleInt,
					&psPMRInt,
					&psPMRLocalImportPMROUT->uiSize,
					&psPMRLocalImportPMROUT->sAlign);
	/* Exit early if bridged call fails */
	if(psPMRLocalImportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRLocalImportPMR_exit;
	}






	psPMRLocalImportPMROUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psPMRLocalImportPMROUT->hPMR,
							(void *) psPMRInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PMRUnrefPMR);
	if (psPMRLocalImportPMROUT->eError != PVRSRV_OK)
	{
		goto PMRLocalImportPMR_exit;
	}




PMRLocalImportPMR_exit:






				{
					/* Unreference the previously looked up handle */
						if(psExtHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hExtHandle,
											PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT);
						}
				}

	if (psPMRLocalImportPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRInt)
		{
			PMRUnrefPMR(psPMRInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgePMRUnrefPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMRUNREFPMR *psPMRUnrefPMRIN,
					  PVRSRV_BRIDGE_OUT_PMRUNREFPMR *psPMRUnrefPMROUT,
					 CONNECTION_DATA *psConnection)
{












	psPMRUnrefPMROUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psPMRUnrefPMRIN->hPMR,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	if ((psPMRUnrefPMROUT->eError != PVRSRV_OK) && (psPMRUnrefPMROUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto PMRUnrefPMR_exit;
	}




PMRUnrefPMR_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgePMRUnrefUnlockPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PMRUNREFUNLOCKPMR *psPMRUnrefUnlockPMRIN,
					  PVRSRV_BRIDGE_OUT_PMRUNREFUNLOCKPMR *psPMRUnrefUnlockPMROUT,
					 CONNECTION_DATA *psConnection)
{












	psPMRUnrefUnlockPMROUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psPMRUnrefUnlockPMRIN->hPMR,
					PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
	if ((psPMRUnrefUnlockPMROUT->eError != PVRSRV_OK) && (psPMRUnrefUnlockPMROUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto PMRUnrefUnlockPMR_exit;
	}




PMRUnrefUnlockPMR_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgePhysmemNewRamBackedPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PHYSMEMNEWRAMBACKEDPMR *psPhysmemNewRamBackedPMRIN,
					  PVRSRV_BRIDGE_OUT_PHYSMEMNEWRAMBACKEDPMR *psPhysmemNewRamBackedPMROUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_UINT32 *ui32MappingTableInt = NULL;
	IMG_CHAR *uiAnnotationInt = NULL;
	PMR * psPMRPtrInt = NULL;




	if (psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks != 0)
	{
		ui32MappingTableInt = OSAllocZMemNoStats(psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks * sizeof(IMG_UINT32));
		if (!ui32MappingTableInt)
		{
			psPhysmemNewRamBackedPMROUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto PhysmemNewRamBackedPMR_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPhysmemNewRamBackedPMRIN->pui32MappingTable, psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32MappingTableInt, psPhysmemNewRamBackedPMRIN->pui32MappingTable,
				psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psPhysmemNewRamBackedPMROUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto PhysmemNewRamBackedPMR_exit;
			}
	if (psPhysmemNewRamBackedPMRIN->ui32AnnotationLength != 0)
	{
		uiAnnotationInt = OSAllocZMemNoStats(psPhysmemNewRamBackedPMRIN->ui32AnnotationLength * sizeof(IMG_CHAR));
		if (!uiAnnotationInt)
		{
			psPhysmemNewRamBackedPMROUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto PhysmemNewRamBackedPMR_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPhysmemNewRamBackedPMRIN->puiAnnotation, psPhysmemNewRamBackedPMRIN->ui32AnnotationLength * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiAnnotationInt, psPhysmemNewRamBackedPMRIN->puiAnnotation,
				psPhysmemNewRamBackedPMRIN->ui32AnnotationLength * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psPhysmemNewRamBackedPMROUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto PhysmemNewRamBackedPMR_exit;
			}


	psPhysmemNewRamBackedPMROUT->eError =
		PhysmemNewRamBackedPMR(psConnection, OSGetDevData(psConnection),
					psPhysmemNewRamBackedPMRIN->uiSize,
					psPhysmemNewRamBackedPMRIN->uiChunkSize,
					psPhysmemNewRamBackedPMRIN->ui32NumPhysChunks,
					psPhysmemNewRamBackedPMRIN->ui32NumVirtChunks,
					ui32MappingTableInt,
					psPhysmemNewRamBackedPMRIN->ui32Log2PageSize,
					psPhysmemNewRamBackedPMRIN->uiFlags,
					psPhysmemNewRamBackedPMRIN->ui32AnnotationLength,
					uiAnnotationInt,
					&psPMRPtrInt);
	/* Exit early if bridged call fails */
	if(psPhysmemNewRamBackedPMROUT->eError != PVRSRV_OK)
	{
		goto PhysmemNewRamBackedPMR_exit;
	}






	psPhysmemNewRamBackedPMROUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psPhysmemNewRamBackedPMROUT->hPMRPtr,
							(void *) psPMRPtrInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PMRUnrefPMR);
	if (psPhysmemNewRamBackedPMROUT->eError != PVRSRV_OK)
	{
		goto PhysmemNewRamBackedPMR_exit;
	}




PhysmemNewRamBackedPMR_exit:


	if (psPhysmemNewRamBackedPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRPtrInt)
		{
			PMRUnrefPMR(psPMRPtrInt);
		}
	}

	if (ui32MappingTableInt)
		OSFreeMemNoStats(ui32MappingTableInt);
	if (uiAnnotationInt)
		OSFreeMemNoStats(uiAnnotationInt);

	return 0;
}


static IMG_INT
PVRSRVBridgePhysmemNewRamBackedLockedPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_PHYSMEMNEWRAMBACKEDLOCKEDPMR *psPhysmemNewRamBackedLockedPMRIN,
					  PVRSRV_BRIDGE_OUT_PHYSMEMNEWRAMBACKEDLOCKEDPMR *psPhysmemNewRamBackedLockedPMROUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_UINT32 *ui32MappingTableInt = NULL;
	IMG_CHAR *uiAnnotationInt = NULL;
	PMR * psPMRPtrInt = NULL;




	if (psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks != 0)
	{
		ui32MappingTableInt = OSAllocZMemNoStats(psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks * sizeof(IMG_UINT32));
		if (!ui32MappingTableInt)
		{
			psPhysmemNewRamBackedLockedPMROUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto PhysmemNewRamBackedLockedPMR_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPhysmemNewRamBackedLockedPMRIN->pui32MappingTable, psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32MappingTableInt, psPhysmemNewRamBackedLockedPMRIN->pui32MappingTable,
				psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psPhysmemNewRamBackedLockedPMROUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto PhysmemNewRamBackedLockedPMR_exit;
			}
	if (psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength != 0)
	{
		uiAnnotationInt = OSAllocZMemNoStats(psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength * sizeof(IMG_CHAR));
		if (!uiAnnotationInt)
		{
			psPhysmemNewRamBackedLockedPMROUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto PhysmemNewRamBackedLockedPMR_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPhysmemNewRamBackedLockedPMRIN->puiAnnotation, psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiAnnotationInt, psPhysmemNewRamBackedLockedPMRIN->puiAnnotation,
				psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psPhysmemNewRamBackedLockedPMROUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto PhysmemNewRamBackedLockedPMR_exit;
			}


	psPhysmemNewRamBackedLockedPMROUT->eError =
		PhysmemNewRamBackedLockedPMR(psConnection, OSGetDevData(psConnection),
					psPhysmemNewRamBackedLockedPMRIN->uiSize,
					psPhysmemNewRamBackedLockedPMRIN->uiChunkSize,
					psPhysmemNewRamBackedLockedPMRIN->ui32NumPhysChunks,
					psPhysmemNewRamBackedLockedPMRIN->ui32NumVirtChunks,
					ui32MappingTableInt,
					psPhysmemNewRamBackedLockedPMRIN->ui32Log2PageSize,
					psPhysmemNewRamBackedLockedPMRIN->uiFlags,
					psPhysmemNewRamBackedLockedPMRIN->ui32AnnotationLength,
					uiAnnotationInt,
					&psPMRPtrInt);
	/* Exit early if bridged call fails */
	if(psPhysmemNewRamBackedLockedPMROUT->eError != PVRSRV_OK)
	{
		goto PhysmemNewRamBackedLockedPMR_exit;
	}






	psPhysmemNewRamBackedLockedPMROUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psPhysmemNewRamBackedLockedPMROUT->hPMRPtr,
							(void *) psPMRPtrInt,
							PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PMRUnrefUnlockPMR);
	if (psPhysmemNewRamBackedLockedPMROUT->eError != PVRSRV_OK)
	{
		goto PhysmemNewRamBackedLockedPMR_exit;
	}




PhysmemNewRamBackedLockedPMR_exit:


	if (psPhysmemNewRamBackedLockedPMROUT->eError != PVRSRV_OK)
	{
		if (psPMRPtrInt)
		{
			PMRUnrefUnlockPMR(psPMRPtrInt);
		}
	}

	if (ui32MappingTableInt)
		OSFreeMemNoStats(ui32MappingTableInt);
	if (uiAnnotationInt)
		OSFreeMemNoStats(uiAnnotationInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntPin(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTPIN *psDevmemIntPinIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTPIN *psDevmemIntPinOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMR = psDevmemIntPinIN->hPMR;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntPinOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psDevmemIntPinOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntPin_exit;
					}
				}

	psDevmemIntPinOUT->eError =
		DevmemIntPin(
					psPMRInt);




DevmemIntPin_exit:






				{
					/* Unreference the previously looked up handle */
						if(psPMRInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntUnpin(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTUNPIN *psDevmemIntUnpinIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTUNPIN *psDevmemIntUnpinOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMR = psDevmemIntUnpinIN->hPMR;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntUnpinOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psDevmemIntUnpinOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntUnpin_exit;
					}
				}

	psDevmemIntUnpinOUT->eError =
		DevmemIntUnpin(
					psPMRInt);




DevmemIntUnpin_exit:






				{
					/* Unreference the previously looked up handle */
						if(psPMRInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntPinValidate(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTPINVALIDATE *psDevmemIntPinValidateIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTPINVALIDATE *psDevmemIntPinValidateOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hMapping = psDevmemIntPinValidateIN->hMapping;
	DEVMEMINT_MAPPING * psMappingInt = NULL;
	IMG_HANDLE hPMR = psDevmemIntPinValidateIN->hPMR;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntPinValidateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psMappingInt,
											hMapping,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING,
											IMG_TRUE);
					if(psDevmemIntPinValidateOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntPinValidate_exit;
					}
				}





				{
					/* Look up the address from the handle */
					psDevmemIntPinValidateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psDevmemIntPinValidateOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntPinValidate_exit;
					}
				}

	psDevmemIntPinValidateOUT->eError =
		DevmemIntPinValidate(
					psMappingInt,
					psPMRInt);




DevmemIntPinValidate_exit:






				{
					/* Unreference the previously looked up handle */
						if(psMappingInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hMapping,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING);
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


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntUnpinInvalidate(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTUNPININVALIDATE *psDevmemIntUnpinInvalidateIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTUNPININVALIDATE *psDevmemIntUnpinInvalidateOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hMapping = psDevmemIntUnpinInvalidateIN->hMapping;
	DEVMEMINT_MAPPING * psMappingInt = NULL;
	IMG_HANDLE hPMR = psDevmemIntUnpinInvalidateIN->hPMR;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntUnpinInvalidateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psMappingInt,
											hMapping,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING,
											IMG_TRUE);
					if(psDevmemIntUnpinInvalidateOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntUnpinInvalidate_exit;
					}
				}





				{
					/* Look up the address from the handle */
					psDevmemIntUnpinInvalidateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psDevmemIntUnpinInvalidateOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntUnpinInvalidate_exit;
					}
				}

	psDevmemIntUnpinInvalidateOUT->eError =
		DevmemIntUnpinInvalidate(
					psMappingInt,
					psPMRInt);




DevmemIntUnpinInvalidate_exit:






				{
					/* Unreference the previously looked up handle */
						if(psMappingInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hMapping,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING);
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


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntCtxCreate(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTCTXCREATE *psDevmemIntCtxCreateIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTCTXCREATE *psDevmemIntCtxCreateOUT,
					 CONNECTION_DATA *psConnection)
{
	DEVMEMINT_CTX * psDevMemServerContextInt = NULL;
	IMG_HANDLE hPrivDataInt = NULL;



	psDevmemIntCtxCreateOUT->hDevMemServerContext = NULL;



	psDevmemIntCtxCreateOUT->eError =
		DevmemIntCtxCreate(psConnection, OSGetDevData(psConnection),
					psDevmemIntCtxCreateIN->bbKernelMemoryCtx,
					&psDevMemServerContextInt,
					&hPrivDataInt,
					&psDevmemIntCtxCreateOUT->ui32CPUCacheLineSize);
	/* Exit early if bridged call fails */
	if(psDevmemIntCtxCreateOUT->eError != PVRSRV_OK)
	{
		goto DevmemIntCtxCreate_exit;
	}






	psDevmemIntCtxCreateOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psDevmemIntCtxCreateOUT->hDevMemServerContext,
							(void *) psDevMemServerContextInt,
							PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&DevmemIntCtxDestroy);
	if (psDevmemIntCtxCreateOUT->eError != PVRSRV_OK)
	{
		goto DevmemIntCtxCreate_exit;
	}






	psDevmemIntCtxCreateOUT->eError = PVRSRVAllocSubHandle(psConnection->psHandleBase,

							&psDevmemIntCtxCreateOUT->hPrivData,
							(void *) hPrivDataInt,
							PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,psDevmemIntCtxCreateOUT->hDevMemServerContext);
	if (psDevmemIntCtxCreateOUT->eError != PVRSRV_OK)
	{
		goto DevmemIntCtxCreate_exit;
	}




DevmemIntCtxCreate_exit:


	if (psDevmemIntCtxCreateOUT->eError != PVRSRV_OK)
	{
		if (psDevmemIntCtxCreateOUT->hDevMemServerContext)
		{


			PVRSRV_ERROR eError = PVRSRVReleaseHandle(psConnection->psHandleBase,
						(IMG_HANDLE) psDevmemIntCtxCreateOUT->hDevMemServerContext,
						PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);

			/* Releasing the handle should free/destroy/release the resource. This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

			/* Avoid freeing/destroying/releasing the resource a second time below */
			psDevMemServerContextInt = NULL;
		}


		if (psDevMemServerContextInt)
		{
			DevmemIntCtxDestroy(psDevMemServerContextInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntCtxDestroy(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTCTXDESTROY *psDevmemIntCtxDestroyIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTCTXDESTROY *psDevmemIntCtxDestroyOUT,
					 CONNECTION_DATA *psConnection)
{












	psDevmemIntCtxDestroyOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDevmemIntCtxDestroyIN->hDevmemServerContext,
					PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
	if ((psDevmemIntCtxDestroyOUT->eError != PVRSRV_OK) && (psDevmemIntCtxDestroyOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto DevmemIntCtxDestroy_exit;
	}




DevmemIntCtxDestroy_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntHeapCreate(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTHEAPCREATE *psDevmemIntHeapCreateIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTHEAPCREATE *psDevmemIntHeapCreateOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevmemCtx = psDevmemIntHeapCreateIN->hDevmemCtx;
	DEVMEMINT_CTX * psDevmemCtxInt = NULL;
	DEVMEMINT_HEAP * psDevmemHeapPtrInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntHeapCreateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psDevmemCtxInt,
											hDevmemCtx,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX,
											IMG_TRUE);
					if(psDevmemIntHeapCreateOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntHeapCreate_exit;
					}
				}

	psDevmemIntHeapCreateOUT->eError =
		DevmemIntHeapCreate(
					psDevmemCtxInt,
					psDevmemIntHeapCreateIN->sHeapBaseAddr,
					psDevmemIntHeapCreateIN->uiHeapLength,
					psDevmemIntHeapCreateIN->ui32Log2DataPageSize,
					&psDevmemHeapPtrInt);
	/* Exit early if bridged call fails */
	if(psDevmemIntHeapCreateOUT->eError != PVRSRV_OK)
	{
		goto DevmemIntHeapCreate_exit;
	}






	psDevmemIntHeapCreateOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psDevmemIntHeapCreateOUT->hDevmemHeapPtr,
							(void *) psDevmemHeapPtrInt,
							PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&DevmemIntHeapDestroy);
	if (psDevmemIntHeapCreateOUT->eError != PVRSRV_OK)
	{
		goto DevmemIntHeapCreate_exit;
	}




DevmemIntHeapCreate_exit:






				{
					/* Unreference the previously looked up handle */
						if(psDevmemCtxInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hDevmemCtx,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
						}
				}

	if (psDevmemIntHeapCreateOUT->eError != PVRSRV_OK)
	{
		if (psDevmemHeapPtrInt)
		{
			DevmemIntHeapDestroy(psDevmemHeapPtrInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntHeapDestroy(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTHEAPDESTROY *psDevmemIntHeapDestroyIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTHEAPDESTROY *psDevmemIntHeapDestroyOUT,
					 CONNECTION_DATA *psConnection)
{












	psDevmemIntHeapDestroyOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDevmemIntHeapDestroyIN->hDevmemHeap,
					PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP);
	if ((psDevmemIntHeapDestroyOUT->eError != PVRSRV_OK) && (psDevmemIntHeapDestroyOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto DevmemIntHeapDestroy_exit;
	}




DevmemIntHeapDestroy_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntMapPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTMAPPMR *psDevmemIntMapPMRIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTMAPPMR *psDevmemIntMapPMROUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevmemServerHeap = psDevmemIntMapPMRIN->hDevmemServerHeap;
	DEVMEMINT_HEAP * psDevmemServerHeapInt = NULL;
	IMG_HANDLE hReservation = psDevmemIntMapPMRIN->hReservation;
	DEVMEMINT_RESERVATION * psReservationInt = NULL;
	IMG_HANDLE hPMR = psDevmemIntMapPMRIN->hPMR;
	PMR * psPMRInt = NULL;
	DEVMEMINT_MAPPING * psMappingInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntMapPMROUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psDevmemServerHeapInt,
											hDevmemServerHeap,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP,
											IMG_TRUE);
					if(psDevmemIntMapPMROUT->eError != PVRSRV_OK)
					{
						goto DevmemIntMapPMR_exit;
					}
				}





				{
					/* Look up the address from the handle */
					psDevmemIntMapPMROUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psReservationInt,
											hReservation,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
											IMG_TRUE);
					if(psDevmemIntMapPMROUT->eError != PVRSRV_OK)
					{
						goto DevmemIntMapPMR_exit;
					}
				}





				{
					/* Look up the address from the handle */
					psDevmemIntMapPMROUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psDevmemIntMapPMROUT->eError != PVRSRV_OK)
					{
						goto DevmemIntMapPMR_exit;
					}
				}

	psDevmemIntMapPMROUT->eError =
		DevmemIntMapPMR(
					psDevmemServerHeapInt,
					psReservationInt,
					psPMRInt,
					psDevmemIntMapPMRIN->uiMapFlags,
					&psMappingInt);
	/* Exit early if bridged call fails */
	if(psDevmemIntMapPMROUT->eError != PVRSRV_OK)
	{
		goto DevmemIntMapPMR_exit;
	}






	psDevmemIntMapPMROUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psDevmemIntMapPMROUT->hMapping,
							(void *) psMappingInt,
							PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&DevmemIntUnmapPMR);
	if (psDevmemIntMapPMROUT->eError != PVRSRV_OK)
	{
		goto DevmemIntMapPMR_exit;
	}




DevmemIntMapPMR_exit:






				{
					/* Unreference the previously looked up handle */
						if(psDevmemServerHeapInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hDevmemServerHeap,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP);
						}
				}





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

	if (psDevmemIntMapPMROUT->eError != PVRSRV_OK)
	{
		if (psMappingInt)
		{
			DevmemIntUnmapPMR(psMappingInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntUnmapPMR(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTUNMAPPMR *psDevmemIntUnmapPMRIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTUNMAPPMR *psDevmemIntUnmapPMROUT,
					 CONNECTION_DATA *psConnection)
{












	psDevmemIntUnmapPMROUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDevmemIntUnmapPMRIN->hMapping,
					PVRSRV_HANDLE_TYPE_DEVMEMINT_MAPPING);
	if ((psDevmemIntUnmapPMROUT->eError != PVRSRV_OK) && (psDevmemIntUnmapPMROUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto DevmemIntUnmapPMR_exit;
	}




DevmemIntUnmapPMR_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntReserveRange(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTRESERVERANGE *psDevmemIntReserveRangeIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTRESERVERANGE *psDevmemIntReserveRangeOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevmemServerHeap = psDevmemIntReserveRangeIN->hDevmemServerHeap;
	DEVMEMINT_HEAP * psDevmemServerHeapInt = NULL;
	DEVMEMINT_RESERVATION * psReservationInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntReserveRangeOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psDevmemServerHeapInt,
											hDevmemServerHeap,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP,
											IMG_TRUE);
					if(psDevmemIntReserveRangeOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntReserveRange_exit;
					}
				}

	psDevmemIntReserveRangeOUT->eError =
		DevmemIntReserveRange(
					psDevmemServerHeapInt,
					psDevmemIntReserveRangeIN->sAddress,
					psDevmemIntReserveRangeIN->uiLength,
					&psReservationInt);
	/* Exit early if bridged call fails */
	if(psDevmemIntReserveRangeOUT->eError != PVRSRV_OK)
	{
		goto DevmemIntReserveRange_exit;
	}






	psDevmemIntReserveRangeOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psDevmemIntReserveRangeOUT->hReservation,
							(void *) psReservationInt,
							PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&DevmemIntUnreserveRange);
	if (psDevmemIntReserveRangeOUT->eError != PVRSRV_OK)
	{
		goto DevmemIntReserveRange_exit;
	}




DevmemIntReserveRange_exit:






				{
					/* Unreference the previously looked up handle */
						if(psDevmemServerHeapInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hDevmemServerHeap,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP);
						}
				}

	if (psDevmemIntReserveRangeOUT->eError != PVRSRV_OK)
	{
		if (psReservationInt)
		{
			DevmemIntUnreserveRange(psReservationInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntUnreserveRange(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTUNRESERVERANGE *psDevmemIntUnreserveRangeIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTUNRESERVERANGE *psDevmemIntUnreserveRangeOUT,
					 CONNECTION_DATA *psConnection)
{












	psDevmemIntUnreserveRangeOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDevmemIntUnreserveRangeIN->hReservation,
					PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
	if ((psDevmemIntUnreserveRangeOUT->eError != PVRSRV_OK) && (psDevmemIntUnreserveRangeOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto DevmemIntUnreserveRange_exit;
	}




DevmemIntUnreserveRange_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeChangeSparseMem(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_CHANGESPARSEMEM *psChangeSparseMemIN,
					  PVRSRV_BRIDGE_OUT_CHANGESPARSEMEM *psChangeSparseMemOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hSrvDevMemHeap = psChangeSparseMemIN->hSrvDevMemHeap;
	DEVMEMINT_HEAP * psSrvDevMemHeapInt = NULL;
	IMG_HANDLE hPMR = psChangeSparseMemIN->hPMR;
	PMR * psPMRInt = NULL;
	IMG_UINT32 *ui32AllocPageIndicesInt = NULL;
	IMG_UINT32 *ui32FreePageIndicesInt = NULL;




	if (psChangeSparseMemIN->ui32AllocPageCount != 0)
	{
		ui32AllocPageIndicesInt = OSAllocZMemNoStats(psChangeSparseMemIN->ui32AllocPageCount * sizeof(IMG_UINT32));
		if (!ui32AllocPageIndicesInt)
		{
			psChangeSparseMemOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ChangeSparseMem_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psChangeSparseMemIN->pui32AllocPageIndices, psChangeSparseMemIN->ui32AllocPageCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32AllocPageIndicesInt, psChangeSparseMemIN->pui32AllocPageIndices,
				psChangeSparseMemIN->ui32AllocPageCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psChangeSparseMemOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto ChangeSparseMem_exit;
			}
	if (psChangeSparseMemIN->ui32FreePageCount != 0)
	{
		ui32FreePageIndicesInt = OSAllocZMemNoStats(psChangeSparseMemIN->ui32FreePageCount * sizeof(IMG_UINT32));
		if (!ui32FreePageIndicesInt)
		{
			psChangeSparseMemOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ChangeSparseMem_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psChangeSparseMemIN->pui32FreePageIndices, psChangeSparseMemIN->ui32FreePageCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32FreePageIndicesInt, psChangeSparseMemIN->pui32FreePageIndices,
				psChangeSparseMemIN->ui32FreePageCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psChangeSparseMemOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto ChangeSparseMem_exit;
			}






				{
					/* Look up the address from the handle */
					psChangeSparseMemOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSrvDevMemHeapInt,
											hSrvDevMemHeap,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP,
											IMG_TRUE);
					if(psChangeSparseMemOUT->eError != PVRSRV_OK)
					{
						goto ChangeSparseMem_exit;
					}
				}





				{
					/* Look up the address from the handle */
					psChangeSparseMemOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psChangeSparseMemOUT->eError != PVRSRV_OK)
					{
						goto ChangeSparseMem_exit;
					}
				}

	psChangeSparseMemOUT->eError =
		DeviceMemChangeSparseServer(
					psSrvDevMemHeapInt,
					psPMRInt,
					psChangeSparseMemIN->ui32AllocPageCount,
					ui32AllocPageIndicesInt,
					psChangeSparseMemIN->ui32FreePageCount,
					ui32FreePageIndicesInt,
					psChangeSparseMemIN->ui32SparseFlags,
					psChangeSparseMemIN->uiFlags,
					psChangeSparseMemIN->sDevVAddr,
					psChangeSparseMemIN->ui64CPUVAddr,
					&psChangeSparseMemOUT->ui32Status);




ChangeSparseMem_exit:






				{
					/* Unreference the previously looked up handle */
						if(psSrvDevMemHeapInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSrvDevMemHeap,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_HEAP);
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

	if (ui32AllocPageIndicesInt)
		OSFreeMemNoStats(ui32AllocPageIndicesInt);
	if (ui32FreePageIndicesInt)
		OSFreeMemNoStats(ui32FreePageIndicesInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntMapPages(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTMAPPAGES *psDevmemIntMapPagesIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTMAPPAGES *psDevmemIntMapPagesOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hReservation = psDevmemIntMapPagesIN->hReservation;
	DEVMEMINT_RESERVATION * psReservationInt = NULL;
	IMG_HANDLE hPMR = psDevmemIntMapPagesIN->hPMR;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntMapPagesOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psReservationInt,
											hReservation,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
											IMG_TRUE);
					if(psDevmemIntMapPagesOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntMapPages_exit;
					}
				}





				{
					/* Look up the address from the handle */
					psDevmemIntMapPagesOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psDevmemIntMapPagesOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntMapPages_exit;
					}
				}

	psDevmemIntMapPagesOUT->eError =
		DevmemIntMapPages(
					psReservationInt,
					psPMRInt,
					psDevmemIntMapPagesIN->ui32PageCount,
					psDevmemIntMapPagesIN->ui32PhysicalPgOffset,
					psDevmemIntMapPagesIN->uiFlags,
					psDevmemIntMapPagesIN->sDevVAddr);




DevmemIntMapPages_exit:






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


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIntUnmapPages(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMINTUNMAPPAGES *psDevmemIntUnmapPagesIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMINTUNMAPPAGES *psDevmemIntUnmapPagesOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hReservation = psDevmemIntUnmapPagesIN->hReservation;
	DEVMEMINT_RESERVATION * psReservationInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIntUnmapPagesOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psReservationInt,
											hReservation,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION,
											IMG_TRUE);
					if(psDevmemIntUnmapPagesOUT->eError != PVRSRV_OK)
					{
						goto DevmemIntUnmapPages_exit;
					}
				}

	psDevmemIntUnmapPagesOUT->eError =
		DevmemIntUnmapPages(
					psReservationInt,
					psDevmemIntUnmapPagesIN->sDevVAddr,
					psDevmemIntUnmapPagesIN->ui32PageCount);




DevmemIntUnmapPages_exit:






				{
					/* Unreference the previously looked up handle */
						if(psReservationInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hReservation,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeDevmemIsVDevAddrValid(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_DEVMEMISVDEVADDRVALID *psDevmemIsVDevAddrValidIN,
					  PVRSRV_BRIDGE_OUT_DEVMEMISVDEVADDRVALID *psDevmemIsVDevAddrValidOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevmemCtx = psDevmemIsVDevAddrValidIN->hDevmemCtx;
	DEVMEMINT_CTX * psDevmemCtxInt = NULL;










				{
					/* Look up the address from the handle */
					psDevmemIsVDevAddrValidOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psDevmemCtxInt,
											hDevmemCtx,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX,
											IMG_TRUE);
					if(psDevmemIsVDevAddrValidOUT->eError != PVRSRV_OK)
					{
						goto DevmemIsVDevAddrValid_exit;
					}
				}

	psDevmemIsVDevAddrValidOUT->eError =
		DevmemIntIsVDevAddrValid(
					psDevmemCtxInt,
					psDevmemIsVDevAddrValidIN->sAddress);




DevmemIsVDevAddrValid_exit:






				{
					/* Unreference the previously looked up handle */
						if(psDevmemCtxInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hDevmemCtx,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_CTX);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeHeapCfgHeapConfigCount(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_HEAPCFGHEAPCONFIGCOUNT *psHeapCfgHeapConfigCountIN,
					  PVRSRV_BRIDGE_OUT_HEAPCFGHEAPCONFIGCOUNT *psHeapCfgHeapConfigCountOUT,
					 CONNECTION_DATA *psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psHeapCfgHeapConfigCountIN);





	psHeapCfgHeapConfigCountOUT->eError =
		HeapCfgHeapConfigCount(psConnection, OSGetDevData(psConnection),
					&psHeapCfgHeapConfigCountOUT->ui32NumHeapConfigs);







	return 0;
}


static IMG_INT
PVRSRVBridgeHeapCfgHeapCount(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_HEAPCFGHEAPCOUNT *psHeapCfgHeapCountIN,
					  PVRSRV_BRIDGE_OUT_HEAPCFGHEAPCOUNT *psHeapCfgHeapCountOUT,
					 CONNECTION_DATA *psConnection)
{






	psHeapCfgHeapCountOUT->eError =
		HeapCfgHeapCount(psConnection, OSGetDevData(psConnection),
					psHeapCfgHeapCountIN->ui32HeapConfigIndex,
					&psHeapCfgHeapCountOUT->ui32NumHeaps);







	return 0;
}


static IMG_INT
PVRSRVBridgeHeapCfgHeapConfigName(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_HEAPCFGHEAPCONFIGNAME *psHeapCfgHeapConfigNameIN,
					  PVRSRV_BRIDGE_OUT_HEAPCFGHEAPCONFIGNAME *psHeapCfgHeapConfigNameOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_CHAR *puiHeapConfigNameInt = NULL;


	psHeapCfgHeapConfigNameOUT->puiHeapConfigName = psHeapCfgHeapConfigNameIN->puiHeapConfigName;


	if (psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz != 0)
	{
		puiHeapConfigNameInt = OSAllocZMemNoStats(psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz * sizeof(IMG_CHAR));
		if (!puiHeapConfigNameInt)
		{
			psHeapCfgHeapConfigNameOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto HeapCfgHeapConfigName_exit;
		}
	}



	psHeapCfgHeapConfigNameOUT->eError =
		HeapCfgHeapConfigName(psConnection, OSGetDevData(psConnection),
					psHeapCfgHeapConfigNameIN->ui32HeapConfigIndex,
					psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz,
					puiHeapConfigNameInt);



	if ( !OSAccessOK(PVR_VERIFY_WRITE, (void*) psHeapCfgHeapConfigNameOUT->puiHeapConfigName, (psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz * sizeof(IMG_CHAR)))
		|| (OSCopyToUser(NULL, psHeapCfgHeapConfigNameOUT->puiHeapConfigName, puiHeapConfigNameInt,
		(psHeapCfgHeapConfigNameIN->ui32HeapConfigNameBufSz * sizeof(IMG_CHAR))) != PVRSRV_OK) )
	{
		psHeapCfgHeapConfigNameOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto HeapCfgHeapConfigName_exit;
	}


HeapCfgHeapConfigName_exit:


	if (puiHeapConfigNameInt)
		OSFreeMemNoStats(puiHeapConfigNameInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeHeapCfgHeapDetails(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_HEAPCFGHEAPDETAILS *psHeapCfgHeapDetailsIN,
					  PVRSRV_BRIDGE_OUT_HEAPCFGHEAPDETAILS *psHeapCfgHeapDetailsOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_CHAR *puiHeapNameOutInt = NULL;


	psHeapCfgHeapDetailsOUT->puiHeapNameOut = psHeapCfgHeapDetailsIN->puiHeapNameOut;


	if (psHeapCfgHeapDetailsIN->ui32HeapNameBufSz != 0)
	{
		puiHeapNameOutInt = OSAllocZMemNoStats(psHeapCfgHeapDetailsIN->ui32HeapNameBufSz * sizeof(IMG_CHAR));
		if (!puiHeapNameOutInt)
		{
			psHeapCfgHeapDetailsOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto HeapCfgHeapDetails_exit;
		}
	}



	psHeapCfgHeapDetailsOUT->eError =
		HeapCfgHeapDetails(psConnection, OSGetDevData(psConnection),
					psHeapCfgHeapDetailsIN->ui32HeapConfigIndex,
					psHeapCfgHeapDetailsIN->ui32HeapIndex,
					psHeapCfgHeapDetailsIN->ui32HeapNameBufSz,
					puiHeapNameOutInt,
					&psHeapCfgHeapDetailsOUT->sDevVAddrBase,
					&psHeapCfgHeapDetailsOUT->uiHeapLength,
					&psHeapCfgHeapDetailsOUT->ui32Log2DataPageSizeOut,
					&psHeapCfgHeapDetailsOUT->ui32Log2ImportAlignmentOut);



	if ( !OSAccessOK(PVR_VERIFY_WRITE, (void*) psHeapCfgHeapDetailsOUT->puiHeapNameOut, (psHeapCfgHeapDetailsIN->ui32HeapNameBufSz * sizeof(IMG_CHAR)))
		|| (OSCopyToUser(NULL, psHeapCfgHeapDetailsOUT->puiHeapNameOut, puiHeapNameOutInt,
		(psHeapCfgHeapDetailsIN->ui32HeapNameBufSz * sizeof(IMG_CHAR))) != PVRSRV_OK) )
	{
		psHeapCfgHeapDetailsOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto HeapCfgHeapDetails_exit;
	}


HeapCfgHeapDetails_exit:


	if (puiHeapNameOutInt)
		OSFreeMemNoStats(puiHeapNameOutInt);

	return 0;
}




/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */

static IMG_BOOL bUseLock = IMG_TRUE;

PVRSRV_ERROR InitMMBridge(void);
PVRSRV_ERROR DeinitMMBridge(void);

/*
 * Register all MM functions with services
 */
PVRSRV_ERROR InitMMBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMREXPORTPMR, PVRSRVBridgePMRExportPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRUNEXPORTPMR, PVRSRVBridgePMRUnexportPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRGETUID, PVRSRVBridgePMRGetUID,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRMAKELOCALIMPORTHANDLE, PVRSRVBridgePMRMakeLocalImportHandle,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRUNMAKELOCALIMPORTHANDLE, PVRSRVBridgePMRUnmakeLocalImportHandle,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRIMPORTPMR, PVRSRVBridgePMRImportPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRLOCALIMPORTPMR, PVRSRVBridgePMRLocalImportPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRUNREFPMR, PVRSRVBridgePMRUnrefPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PMRUNREFUNLOCKPMR, PVRSRVBridgePMRUnrefUnlockPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR, PVRSRVBridgePhysmemNewRamBackedPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDLOCKEDPMR, PVRSRVBridgePhysmemNewRamBackedLockedPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTPIN, PVRSRVBridgeDevmemIntPin,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTUNPIN, PVRSRVBridgeDevmemIntUnpin,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTPINVALIDATE, PVRSRVBridgeDevmemIntPinValidate,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTUNPININVALIDATE, PVRSRVBridgeDevmemIntUnpinInvalidate,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTCTXCREATE, PVRSRVBridgeDevmemIntCtxCreate,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTCTXDESTROY, PVRSRVBridgeDevmemIntCtxDestroy,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTHEAPCREATE, PVRSRVBridgeDevmemIntHeapCreate,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY, PVRSRVBridgeDevmemIntHeapDestroy,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTMAPPMR, PVRSRVBridgeDevmemIntMapPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTUNMAPPMR, PVRSRVBridgeDevmemIntUnmapPMR,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTRESERVERANGE, PVRSRVBridgeDevmemIntReserveRange,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTUNRESERVERANGE, PVRSRVBridgeDevmemIntUnreserveRange,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_CHANGESPARSEMEM, PVRSRVBridgeChangeSparseMem,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTMAPPAGES, PVRSRVBridgeDevmemIntMapPages,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMINTUNMAPPAGES, PVRSRVBridgeDevmemIntUnmapPages,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_DEVMEMISVDEVADDRVALID, PVRSRVBridgeDevmemIsVDevAddrValid,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_HEAPCFGHEAPCONFIGCOUNT, PVRSRVBridgeHeapCfgHeapConfigCount,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_HEAPCFGHEAPCOUNT, PVRSRVBridgeHeapCfgHeapCount,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_HEAPCFGHEAPCONFIGNAME, PVRSRVBridgeHeapCfgHeapConfigName,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_MM, PVRSRV_BRIDGE_MM_HEAPCFGHEAPDETAILS, PVRSRVBridgeHeapCfgHeapDetails,
					NULL, bUseLock);


	return PVRSRV_OK;
}

/*
 * Unregister all mm functions with services
 */
PVRSRV_ERROR DeinitMMBridge(void)
{
	return PVRSRV_OK;
}
