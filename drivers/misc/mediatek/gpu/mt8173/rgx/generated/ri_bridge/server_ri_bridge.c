/*************************************************************************/ /*!
@File
@Title          Server bridge for ri
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for ri
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

#include "ri_server.h"


#include "common_ri_bridge.h"

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
PVRSRVBridgeRIWritePMREntry(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIWRITEPMRENTRY *psRIWritePMREntryIN,
					  PVRSRV_BRIDGE_OUT_RIWRITEPMRENTRY *psRIWritePMREntryOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMRHandle = psRIWritePMREntryIN->hPMRHandle;
	PMR * psPMRHandleInt = NULL;
	IMG_CHAR *uiTextAInt = NULL;




	if (psRIWritePMREntryIN->ui32TextASize != 0)
	{
		uiTextAInt = OSAllocZMemNoStats(psRIWritePMREntryIN->ui32TextASize * sizeof(IMG_CHAR));
		if (!uiTextAInt)
		{
			psRIWritePMREntryOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RIWritePMREntry_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRIWritePMREntryIN->puiTextA, psRIWritePMREntryIN->ui32TextASize * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiTextAInt, psRIWritePMREntryIN->puiTextA,
				psRIWritePMREntryIN->ui32TextASize * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psRIWritePMREntryOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RIWritePMREntry_exit;
			}






				{
					/* Look up the address from the handle */
					psRIWritePMREntryOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRHandleInt,
											hPMRHandle,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psRIWritePMREntryOUT->eError != PVRSRV_OK)
					{
						goto RIWritePMREntry_exit;
					}
				}

	psRIWritePMREntryOUT->eError =
		RIWritePMREntryKM(
					psPMRHandleInt,
					psRIWritePMREntryIN->ui32TextASize,
					uiTextAInt,
					psRIWritePMREntryIN->ui64LogicalSize);




RIWritePMREntry_exit:






				{
					/* Unreference the previously looked up handle */
						if(psPMRHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMRHandle,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}

	if (uiTextAInt)
		OSFreeMemNoStats(uiTextAInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeRIWriteMEMDESCEntry(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIWRITEMEMDESCENTRY *psRIWriteMEMDESCEntryIN,
					  PVRSRV_BRIDGE_OUT_RIWRITEMEMDESCENTRY *psRIWriteMEMDESCEntryOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMRHandle = psRIWriteMEMDESCEntryIN->hPMRHandle;
	PMR * psPMRHandleInt = NULL;
	IMG_CHAR *uiTextBInt = NULL;
	RI_HANDLE psRIHandleInt = NULL;




	if (psRIWriteMEMDESCEntryIN->ui32TextBSize != 0)
	{
		uiTextBInt = OSAllocZMemNoStats(psRIWriteMEMDESCEntryIN->ui32TextBSize * sizeof(IMG_CHAR));
		if (!uiTextBInt)
		{
			psRIWriteMEMDESCEntryOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RIWriteMEMDESCEntry_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRIWriteMEMDESCEntryIN->puiTextB, psRIWriteMEMDESCEntryIN->ui32TextBSize * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiTextBInt, psRIWriteMEMDESCEntryIN->puiTextB,
				psRIWriteMEMDESCEntryIN->ui32TextBSize * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psRIWriteMEMDESCEntryOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RIWriteMEMDESCEntry_exit;
			}






				{
					/* Look up the address from the handle */
					psRIWriteMEMDESCEntryOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRHandleInt,
											hPMRHandle,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psRIWriteMEMDESCEntryOUT->eError != PVRSRV_OK)
					{
						goto RIWriteMEMDESCEntry_exit;
					}
				}

	psRIWriteMEMDESCEntryOUT->eError =
		RIWriteMEMDESCEntryKM(
					psPMRHandleInt,
					psRIWriteMEMDESCEntryIN->ui32TextBSize,
					uiTextBInt,
					psRIWriteMEMDESCEntryIN->ui64Offset,
					psRIWriteMEMDESCEntryIN->ui64Size,
					psRIWriteMEMDESCEntryIN->ui64BackedSize,
					psRIWriteMEMDESCEntryIN->bIsImport,
					psRIWriteMEMDESCEntryIN->bIsExportable,
					&psRIHandleInt);
	/* Exit early if bridged call fails */
	if(psRIWriteMEMDESCEntryOUT->eError != PVRSRV_OK)
	{
		goto RIWriteMEMDESCEntry_exit;
	}






	psRIWriteMEMDESCEntryOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRIWriteMEMDESCEntryOUT->hRIHandle,
							(void *) psRIHandleInt,
							PVRSRV_HANDLE_TYPE_RI_HANDLE,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&RIDeleteMEMDESCEntryKM);
	if (psRIWriteMEMDESCEntryOUT->eError != PVRSRV_OK)
	{
		goto RIWriteMEMDESCEntry_exit;
	}




RIWriteMEMDESCEntry_exit:






				{
					/* Unreference the previously looked up handle */
						if(psPMRHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMRHandle,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}

	if (psRIWriteMEMDESCEntryOUT->eError != PVRSRV_OK)
	{
		if (psRIHandleInt)
		{
			RIDeleteMEMDESCEntryKM(psRIHandleInt);
		}
	}

	if (uiTextBInt)
		OSFreeMemNoStats(uiTextBInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeRIWriteProcListEntry(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIWRITEPROCLISTENTRY *psRIWriteProcListEntryIN,
					  PVRSRV_BRIDGE_OUT_RIWRITEPROCLISTENTRY *psRIWriteProcListEntryOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_CHAR *uiTextBInt = NULL;
	RI_HANDLE psRIHandleInt = NULL;




	if (psRIWriteProcListEntryIN->ui32TextBSize != 0)
	{
		uiTextBInt = OSAllocZMemNoStats(psRIWriteProcListEntryIN->ui32TextBSize * sizeof(IMG_CHAR));
		if (!uiTextBInt)
		{
			psRIWriteProcListEntryOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RIWriteProcListEntry_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRIWriteProcListEntryIN->puiTextB, psRIWriteProcListEntryIN->ui32TextBSize * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiTextBInt, psRIWriteProcListEntryIN->puiTextB,
				psRIWriteProcListEntryIN->ui32TextBSize * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psRIWriteProcListEntryOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RIWriteProcListEntry_exit;
			}


	psRIWriteProcListEntryOUT->eError =
		RIWriteProcListEntryKM(
					psRIWriteProcListEntryIN->ui32TextBSize,
					uiTextBInt,
					psRIWriteProcListEntryIN->ui64Size,
					psRIWriteProcListEntryIN->ui64BackedSize,
					psRIWriteProcListEntryIN->ui64DevVAddr,
					&psRIHandleInt);
	/* Exit early if bridged call fails */
	if(psRIWriteProcListEntryOUT->eError != PVRSRV_OK)
	{
		goto RIWriteProcListEntry_exit;
	}






	psRIWriteProcListEntryOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRIWriteProcListEntryOUT->hRIHandle,
							(void *) psRIHandleInt,
							PVRSRV_HANDLE_TYPE_RI_HANDLE,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&RIDeleteMEMDESCEntryKM);
	if (psRIWriteProcListEntryOUT->eError != PVRSRV_OK)
	{
		goto RIWriteProcListEntry_exit;
	}




RIWriteProcListEntry_exit:


	if (psRIWriteProcListEntryOUT->eError != PVRSRV_OK)
	{
		if (psRIHandleInt)
		{
			RIDeleteMEMDESCEntryKM(psRIHandleInt);
		}
	}

	if (uiTextBInt)
		OSFreeMemNoStats(uiTextBInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeRIUpdateMEMDESCAddr(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIUPDATEMEMDESCADDR *psRIUpdateMEMDESCAddrIN,
					  PVRSRV_BRIDGE_OUT_RIUPDATEMEMDESCADDR *psRIUpdateMEMDESCAddrOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hRIHandle = psRIUpdateMEMDESCAddrIN->hRIHandle;
	RI_HANDLE psRIHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psRIUpdateMEMDESCAddrOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psRIHandleInt,
											hRIHandle,
											PVRSRV_HANDLE_TYPE_RI_HANDLE,
											IMG_TRUE);
					if(psRIUpdateMEMDESCAddrOUT->eError != PVRSRV_OK)
					{
						goto RIUpdateMEMDESCAddr_exit;
					}
				}

	psRIUpdateMEMDESCAddrOUT->eError =
		RIUpdateMEMDESCAddrKM(
					psRIHandleInt,
					psRIUpdateMEMDESCAddrIN->sAddr);




RIUpdateMEMDESCAddr_exit:






				{
					/* Unreference the previously looked up handle */
						if(psRIHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hRIHandle,
											PVRSRV_HANDLE_TYPE_RI_HANDLE);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeRIUpdateMEMDESCPinning(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIUPDATEMEMDESCPINNING *psRIUpdateMEMDESCPinningIN,
					  PVRSRV_BRIDGE_OUT_RIUPDATEMEMDESCPINNING *psRIUpdateMEMDESCPinningOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hRIHandle = psRIUpdateMEMDESCPinningIN->hRIHandle;
	RI_HANDLE psRIHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psRIUpdateMEMDESCPinningOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psRIHandleInt,
											hRIHandle,
											PVRSRV_HANDLE_TYPE_RI_HANDLE,
											IMG_TRUE);
					if(psRIUpdateMEMDESCPinningOUT->eError != PVRSRV_OK)
					{
						goto RIUpdateMEMDESCPinning_exit;
					}
				}

	psRIUpdateMEMDESCPinningOUT->eError =
		RIUpdateMEMDESCPinningKM(
					psRIHandleInt,
					psRIUpdateMEMDESCPinningIN->bIsPinned);




RIUpdateMEMDESCPinning_exit:






				{
					/* Unreference the previously looked up handle */
						if(psRIHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hRIHandle,
											PVRSRV_HANDLE_TYPE_RI_HANDLE);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeRIUpdateMEMDESCBacking(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIUPDATEMEMDESCBACKING *psRIUpdateMEMDESCBackingIN,
					  PVRSRV_BRIDGE_OUT_RIUPDATEMEMDESCBACKING *psRIUpdateMEMDESCBackingOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hRIHandle = psRIUpdateMEMDESCBackingIN->hRIHandle;
	RI_HANDLE psRIHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psRIUpdateMEMDESCBackingOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psRIHandleInt,
											hRIHandle,
											PVRSRV_HANDLE_TYPE_RI_HANDLE,
											IMG_TRUE);
					if(psRIUpdateMEMDESCBackingOUT->eError != PVRSRV_OK)
					{
						goto RIUpdateMEMDESCBacking_exit;
					}
				}

	psRIUpdateMEMDESCBackingOUT->eError =
		RIUpdateMEMDESCBackingKM(
					psRIHandleInt,
					psRIUpdateMEMDESCBackingIN->i32NumModified);




RIUpdateMEMDESCBacking_exit:






				{
					/* Unreference the previously looked up handle */
						if(psRIHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hRIHandle,
											PVRSRV_HANDLE_TYPE_RI_HANDLE);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeRIDeleteMEMDESCEntry(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIDELETEMEMDESCENTRY *psRIDeleteMEMDESCEntryIN,
					  PVRSRV_BRIDGE_OUT_RIDELETEMEMDESCENTRY *psRIDeleteMEMDESCEntryOUT,
					 CONNECTION_DATA *psConnection)
{












	psRIDeleteMEMDESCEntryOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRIDeleteMEMDESCEntryIN->hRIHandle,
					PVRSRV_HANDLE_TYPE_RI_HANDLE);
	if ((psRIDeleteMEMDESCEntryOUT->eError != PVRSRV_OK) && (psRIDeleteMEMDESCEntryOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RIDeleteMEMDESCEntry_exit;
	}




RIDeleteMEMDESCEntry_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRIDumpList(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIDUMPLIST *psRIDumpListIN,
					  PVRSRV_BRIDGE_OUT_RIDUMPLIST *psRIDumpListOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMRHandle = psRIDumpListIN->hPMRHandle;
	PMR * psPMRHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psRIDumpListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRHandleInt,
											hPMRHandle,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psRIDumpListOUT->eError != PVRSRV_OK)
					{
						goto RIDumpList_exit;
					}
				}

	psRIDumpListOUT->eError =
		RIDumpListKM(
					psPMRHandleInt);




RIDumpList_exit:






				{
					/* Unreference the previously looked up handle */
						if(psPMRHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMRHandle,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeRIDumpAll(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIDUMPALL *psRIDumpAllIN,
					  PVRSRV_BRIDGE_OUT_RIDUMPALL *psRIDumpAllOUT,
					 CONNECTION_DATA *psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psRIDumpAllIN);





	psRIDumpAllOUT->eError =
		RIDumpAllKM(
					);







	return 0;
}


static IMG_INT
PVRSRVBridgeRIDumpProcess(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RIDUMPPROCESS *psRIDumpProcessIN,
					  PVRSRV_BRIDGE_OUT_RIDUMPPROCESS *psRIDumpProcessOUT,
					 CONNECTION_DATA *psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psConnection);





	psRIDumpProcessOUT->eError =
		RIDumpProcessKM(
					psRIDumpProcessIN->ui32Pid);







	return 0;
}




/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */

static IMG_BOOL bUseLock = IMG_TRUE;

PVRSRV_ERROR InitRIBridge(void);
PVRSRV_ERROR DeinitRIBridge(void);

/*
 * Register all RI functions with services
 */
PVRSRV_ERROR InitRIBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIWRITEPMRENTRY, PVRSRVBridgeRIWritePMREntry,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIWRITEMEMDESCENTRY, PVRSRVBridgeRIWriteMEMDESCEntry,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIWRITEPROCLISTENTRY, PVRSRVBridgeRIWriteProcListEntry,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIUPDATEMEMDESCADDR, PVRSRVBridgeRIUpdateMEMDESCAddr,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIUPDATEMEMDESCPINNING, PVRSRVBridgeRIUpdateMEMDESCPinning,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIUPDATEMEMDESCBACKING, PVRSRVBridgeRIUpdateMEMDESCBacking,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIDELETEMEMDESCENTRY, PVRSRVBridgeRIDeleteMEMDESCEntry,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIDUMPLIST, PVRSRVBridgeRIDumpList,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIDUMPALL, PVRSRVBridgeRIDumpAll,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RI, PVRSRV_BRIDGE_RI_RIDUMPPROCESS, PVRSRVBridgeRIDumpProcess,
					NULL, bUseLock);


	return PVRSRV_OK;
}

/*
 * Unregister all ri functions with services
 */
PVRSRV_ERROR DeinitRIBridge(void)
{
	return PVRSRV_OK;
}
