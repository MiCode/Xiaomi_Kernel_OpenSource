/*************************************************************************/ /*!
@File
@Title          Server bridge for sync
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for sync
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

#include "sync.h"
#include "sync_server.h"
#include "pdump.h"


#include "common_sync_bridge.h"

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
PVRSRVBridgeAllocSyncPrimitiveBlock(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_ALLOCSYNCPRIMITIVEBLOCK *psAllocSyncPrimitiveBlockIN,
					  PVRSRV_BRIDGE_OUT_ALLOCSYNCPRIMITIVEBLOCK *psAllocSyncPrimitiveBlockOUT,
					 CONNECTION_DATA *psConnection)
{
	SYNC_PRIMITIVE_BLOCK * psSyncHandleInt = NULL;
	PMR * pshSyncPMRInt = NULL;

	PVR_UNREFERENCED_PARAMETER(psAllocSyncPrimitiveBlockIN);


	psAllocSyncPrimitiveBlockOUT->hSyncHandle = NULL;



	psAllocSyncPrimitiveBlockOUT->eError =
		PVRSRVAllocSyncPrimitiveBlockKM(psConnection, OSGetDevData(psConnection),
					&psSyncHandleInt,
					&psAllocSyncPrimitiveBlockOUT->ui32SyncPrimVAddr,
					&psAllocSyncPrimitiveBlockOUT->ui32SyncPrimBlockSize,
					&pshSyncPMRInt);
	/* Exit early if bridged call fails */
	if(psAllocSyncPrimitiveBlockOUT->eError != PVRSRV_OK)
	{
		goto AllocSyncPrimitiveBlock_exit;
	}






	psAllocSyncPrimitiveBlockOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psAllocSyncPrimitiveBlockOUT->hSyncHandle,
							(void *) psSyncHandleInt,
							PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PVRSRVFreeSyncPrimitiveBlockKM);
	if (psAllocSyncPrimitiveBlockOUT->eError != PVRSRV_OK)
	{
		goto AllocSyncPrimitiveBlock_exit;
	}






	psAllocSyncPrimitiveBlockOUT->eError = PVRSRVAllocSubHandle(psConnection->psHandleBase,

							&psAllocSyncPrimitiveBlockOUT->hhSyncPMR,
							(void *) pshSyncPMRInt,
							PVRSRV_HANDLE_TYPE_PMR_LOCAL_EXPORT_HANDLE,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							,psAllocSyncPrimitiveBlockOUT->hSyncHandle);
	if (psAllocSyncPrimitiveBlockOUT->eError != PVRSRV_OK)
	{
		goto AllocSyncPrimitiveBlock_exit;
	}




AllocSyncPrimitiveBlock_exit:


	if (psAllocSyncPrimitiveBlockOUT->eError != PVRSRV_OK)
	{
		if (psAllocSyncPrimitiveBlockOUT->hSyncHandle)
		{


			PVRSRV_ERROR eError = PVRSRVReleaseHandle(psConnection->psHandleBase,
						(IMG_HANDLE) psAllocSyncPrimitiveBlockOUT->hSyncHandle,
						PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);

			/* Releasing the handle should free/destroy/release the resource. This should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

			/* Avoid freeing/destroying/releasing the resource a second time below */
			psSyncHandleInt = NULL;
		}


		if (psSyncHandleInt)
		{
			PVRSRVFreeSyncPrimitiveBlockKM(psSyncHandleInt);
		}
	}


	return 0;
}


static IMG_INT
PVRSRVBridgeFreeSyncPrimitiveBlock(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_FREESYNCPRIMITIVEBLOCK *psFreeSyncPrimitiveBlockIN,
					  PVRSRV_BRIDGE_OUT_FREESYNCPRIMITIVEBLOCK *psFreeSyncPrimitiveBlockOUT,
					 CONNECTION_DATA *psConnection)
{












	psFreeSyncPrimitiveBlockOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psFreeSyncPrimitiveBlockIN->hSyncHandle,
					PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
	if ((psFreeSyncPrimitiveBlockOUT->eError != PVRSRV_OK) && (psFreeSyncPrimitiveBlockOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto FreeSyncPrimitiveBlock_exit;
	}




FreeSyncPrimitiveBlock_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeSyncPrimSet(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMSET *psSyncPrimSetIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMSET *psSyncPrimSetOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hSyncHandle = psSyncPrimSetIN->hSyncHandle;
	SYNC_PRIMITIVE_BLOCK * psSyncHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psSyncPrimSetOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psSyncPrimSetOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimSet_exit;
					}
				}

	psSyncPrimSetOUT->eError =
		PVRSRVSyncPrimSetKM(
					psSyncHandleInt,
					psSyncPrimSetIN->ui32Index,
					psSyncPrimSetIN->ui32Value);




SyncPrimSet_exit:






				{
					/* Unreference the previously looked up handle */
						if(psSyncHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeServerSyncPrimSet(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SERVERSYNCPRIMSET *psServerSyncPrimSetIN,
					  PVRSRV_BRIDGE_OUT_SERVERSYNCPRIMSET *psServerSyncPrimSetOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hSyncHandle = psServerSyncPrimSetIN->hSyncHandle;
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psServerSyncPrimSetOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
											IMG_TRUE);
					if(psServerSyncPrimSetOUT->eError != PVRSRV_OK)
					{
						goto ServerSyncPrimSet_exit;
					}
				}

	psServerSyncPrimSetOUT->eError =
		PVRSRVServerSyncPrimSetKM(
					psSyncHandleInt,
					psServerSyncPrimSetIN->ui32Value);




ServerSyncPrimSet_exit:






				{
					/* Unreference the previously looked up handle */
						if(psSyncHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
						}
				}


	return 0;
}


#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
static IMG_INT
PVRSRVBridgeSyncRecordRemoveByHandle(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCRECORDREMOVEBYHANDLE *psSyncRecordRemoveByHandleIN,
					  PVRSRV_BRIDGE_OUT_SYNCRECORDREMOVEBYHANDLE *psSyncRecordRemoveByHandleOUT,
					 CONNECTION_DATA *psConnection)
{












	psSyncRecordRemoveByHandleOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psSyncRecordRemoveByHandleIN->hhRecord,
					PVRSRV_HANDLE_TYPE_SYNC_RECORD_HANDLE);
	if ((psSyncRecordRemoveByHandleOUT->eError != PVRSRV_OK) && (psSyncRecordRemoveByHandleOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto SyncRecordRemoveByHandle_exit;
	}




SyncRecordRemoveByHandle_exit:



	return 0;
}

#else
#define PVRSRVBridgeSyncRecordRemoveByHandle NULL
#endif

#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
static IMG_INT
PVRSRVBridgeSyncRecordAdd(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCRECORDADD *psSyncRecordAddIN,
					  PVRSRV_BRIDGE_OUT_SYNCRECORDADD *psSyncRecordAddOUT,
					 CONNECTION_DATA *psConnection)
{
	SYNC_RECORD_HANDLE pshRecordInt = NULL;
	IMG_HANDLE hhServerSyncPrimBlock = psSyncRecordAddIN->hhServerSyncPrimBlock;
	SYNC_PRIMITIVE_BLOCK * pshServerSyncPrimBlockInt = NULL;
	IMG_CHAR *uiClassNameInt = NULL;




	if (psSyncRecordAddIN->ui32ClassNameSize != 0)
	{
		uiClassNameInt = OSAllocZMemNoStats(psSyncRecordAddIN->ui32ClassNameSize * sizeof(IMG_CHAR));
		if (!uiClassNameInt)
		{
			psSyncRecordAddOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncRecordAdd_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncRecordAddIN->puiClassName, psSyncRecordAddIN->ui32ClassNameSize * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiClassNameInt, psSyncRecordAddIN->puiClassName,
				psSyncRecordAddIN->ui32ClassNameSize * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psSyncRecordAddOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncRecordAdd_exit;
			}






				{
					/* Look up the address from the handle */
					psSyncRecordAddOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &pshServerSyncPrimBlockInt,
											hhServerSyncPrimBlock,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psSyncRecordAddOUT->eError != PVRSRV_OK)
					{
						goto SyncRecordAdd_exit;
					}
				}

	psSyncRecordAddOUT->eError =
		PVRSRVSyncRecordAddKM(psConnection, OSGetDevData(psConnection),
					&pshRecordInt,
					pshServerSyncPrimBlockInt,
					psSyncRecordAddIN->ui32ui32FwBlockAddr,
					psSyncRecordAddIN->ui32ui32SyncOffset,
					psSyncRecordAddIN->bbServerSync,
					psSyncRecordAddIN->ui32ClassNameSize,
					uiClassNameInt);
	/* Exit early if bridged call fails */
	if(psSyncRecordAddOUT->eError != PVRSRV_OK)
	{
		goto SyncRecordAdd_exit;
	}






	psSyncRecordAddOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psSyncRecordAddOUT->hhRecord,
							(void *) pshRecordInt,
							PVRSRV_HANDLE_TYPE_SYNC_RECORD_HANDLE,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							,(PFN_HANDLE_RELEASE)&PVRSRVSyncRecordRemoveByHandleKM);
	if (psSyncRecordAddOUT->eError != PVRSRV_OK)
	{
		goto SyncRecordAdd_exit;
	}




SyncRecordAdd_exit:






				{
					/* Unreference the previously looked up handle */
						if(pshServerSyncPrimBlockInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hhServerSyncPrimBlock,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}

	if (psSyncRecordAddOUT->eError != PVRSRV_OK)
	{
		if (pshRecordInt)
		{
			PVRSRVSyncRecordRemoveByHandleKM(pshRecordInt);
		}
	}

	if (uiClassNameInt)
		OSFreeMemNoStats(uiClassNameInt);

	return 0;
}

#else
#define PVRSRVBridgeSyncRecordAdd NULL
#endif

#if !defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
static IMG_INT
PVRSRVBridgeSyncAllocEvent(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCALLOCEVENT *psSyncAllocEventIN,
					  PVRSRV_BRIDGE_OUT_SYNCALLOCEVENT *psSyncAllocEventOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_CHAR *uiClassNameInt = NULL;

	PVR_UNREFERENCED_PARAMETER(psConnection);



	if (psSyncAllocEventIN->ui32ClassNameSize != 0)
	{
		uiClassNameInt = OSAllocZMemNoStats(psSyncAllocEventIN->ui32ClassNameSize * sizeof(IMG_CHAR));
		if (!uiClassNameInt)
		{
			psSyncAllocEventOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncAllocEvent_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncAllocEventIN->puiClassName, psSyncAllocEventIN->ui32ClassNameSize * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiClassNameInt, psSyncAllocEventIN->puiClassName,
				psSyncAllocEventIN->ui32ClassNameSize * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psSyncAllocEventOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncAllocEvent_exit;
			}


	psSyncAllocEventOUT->eError =
		PVRSRVSyncAllocEventKM(
					psSyncAllocEventIN->bServerSync,
					psSyncAllocEventIN->ui32FWAddr,
					psSyncAllocEventIN->ui32ClassNameSize,
					uiClassNameInt);




SyncAllocEvent_exit:


	if (uiClassNameInt)
		OSFreeMemNoStats(uiClassNameInt);

	return 0;
}

#else
#define PVRSRVBridgeSyncAllocEvent NULL
#endif

static IMG_INT
PVRSRVBridgeServerSyncAlloc(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SERVERSYNCALLOC *psServerSyncAllocIN,
					  PVRSRV_BRIDGE_OUT_SERVERSYNCALLOC *psServerSyncAllocOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = NULL;
	IMG_CHAR *uiClassNameInt = NULL;




	if (psServerSyncAllocIN->ui32ClassNameSize != 0)
	{
		uiClassNameInt = OSAllocZMemNoStats(psServerSyncAllocIN->ui32ClassNameSize * sizeof(IMG_CHAR));
		if (!uiClassNameInt)
		{
			psServerSyncAllocOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ServerSyncAlloc_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psServerSyncAllocIN->puiClassName, psServerSyncAllocIN->ui32ClassNameSize * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiClassNameInt, psServerSyncAllocIN->puiClassName,
				psServerSyncAllocIN->ui32ClassNameSize * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psServerSyncAllocOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto ServerSyncAlloc_exit;
			}


	psServerSyncAllocOUT->eError =
		PVRSRVServerSyncAllocKM(psConnection, OSGetDevData(psConnection),
					&psSyncHandleInt,
					&psServerSyncAllocOUT->ui32SyncPrimVAddr,
					psServerSyncAllocIN->ui32ClassNameSize,
					uiClassNameInt);
	/* Exit early if bridged call fails */
	if(psServerSyncAllocOUT->eError != PVRSRV_OK)
	{
		goto ServerSyncAlloc_exit;
	}






	psServerSyncAllocOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psServerSyncAllocOUT->hSyncHandle,
							(void *) psSyncHandleInt,
							PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PVRSRVServerSyncFreeKM);
	if (psServerSyncAllocOUT->eError != PVRSRV_OK)
	{
		goto ServerSyncAlloc_exit;
	}




ServerSyncAlloc_exit:


	if (psServerSyncAllocOUT->eError != PVRSRV_OK)
	{
		if (psSyncHandleInt)
		{
			PVRSRVServerSyncFreeKM(psSyncHandleInt);
		}
	}

	if (uiClassNameInt)
		OSFreeMemNoStats(uiClassNameInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeServerSyncFree(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SERVERSYNCFREE *psServerSyncFreeIN,
					  PVRSRV_BRIDGE_OUT_SERVERSYNCFREE *psServerSyncFreeOUT,
					 CONNECTION_DATA *psConnection)
{












	psServerSyncFreeOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psServerSyncFreeIN->hSyncHandle,
					PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
	if ((psServerSyncFreeOUT->eError != PVRSRV_OK) && (psServerSyncFreeOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto ServerSyncFree_exit;
	}




ServerSyncFree_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeServerSyncQueueHWOp(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SERVERSYNCQUEUEHWOP *psServerSyncQueueHWOpIN,
					  PVRSRV_BRIDGE_OUT_SERVERSYNCQUEUEHWOP *psServerSyncQueueHWOpOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hSyncHandle = psServerSyncQueueHWOpIN->hSyncHandle;
	SERVER_SYNC_PRIMITIVE * psSyncHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psServerSyncQueueHWOpOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
											IMG_TRUE);
					if(psServerSyncQueueHWOpOUT->eError != PVRSRV_OK)
					{
						goto ServerSyncQueueHWOp_exit;
					}
				}

	psServerSyncQueueHWOpOUT->eError =
		PVRSRVServerSyncQueueHWOpKM(
					psSyncHandleInt,
					psServerSyncQueueHWOpIN->bbUpdate,
					&psServerSyncQueueHWOpOUT->ui32FenceValue,
					&psServerSyncQueueHWOpOUT->ui32UpdateValue);




ServerSyncQueueHWOp_exit:






				{
					/* Unreference the previously looked up handle */
						if(psSyncHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeServerSyncGetStatus(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SERVERSYNCGETSTATUS *psServerSyncGetStatusIN,
					  PVRSRV_BRIDGE_OUT_SERVERSYNCGETSTATUS *psServerSyncGetStatusOUT,
					 CONNECTION_DATA *psConnection)
{
	SERVER_SYNC_PRIMITIVE * *psSyncHandleInt = NULL;
	IMG_HANDLE *hSyncHandleInt2 = NULL;
	IMG_UINT32 *pui32UIDInt = NULL;
	IMG_UINT32 *pui32FWAddrInt = NULL;
	IMG_UINT32 *pui32CurrentOpInt = NULL;
	IMG_UINT32 *pui32NextOpInt = NULL;


	psServerSyncGetStatusOUT->pui32UID = psServerSyncGetStatusIN->pui32UID;
	psServerSyncGetStatusOUT->pui32FWAddr = psServerSyncGetStatusIN->pui32FWAddr;
	psServerSyncGetStatusOUT->pui32CurrentOp = psServerSyncGetStatusIN->pui32CurrentOp;
	psServerSyncGetStatusOUT->pui32NextOp = psServerSyncGetStatusIN->pui32NextOp;


	if (psServerSyncGetStatusIN->ui32SyncCount != 0)
	{
		psSyncHandleInt = OSAllocZMemNoStats(psServerSyncGetStatusIN->ui32SyncCount * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psSyncHandleInt)
		{
			psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ServerSyncGetStatus_exit;
		}
		hSyncHandleInt2 = OSAllocMemNoStats(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_HANDLE));
		if (!hSyncHandleInt2)
		{
			psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ServerSyncGetStatus_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psServerSyncGetStatusIN->phSyncHandle, psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hSyncHandleInt2, psServerSyncGetStatusIN->phSyncHandle,
				psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto ServerSyncGetStatus_exit;
			}
	if (psServerSyncGetStatusIN->ui32SyncCount != 0)
	{
		pui32UIDInt = OSAllocZMemNoStats(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32));
		if (!pui32UIDInt)
		{
			psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ServerSyncGetStatus_exit;
		}
	}

	if (psServerSyncGetStatusIN->ui32SyncCount != 0)
	{
		pui32FWAddrInt = OSAllocZMemNoStats(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32));
		if (!pui32FWAddrInt)
		{
			psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ServerSyncGetStatus_exit;
		}
	}

	if (psServerSyncGetStatusIN->ui32SyncCount != 0)
	{
		pui32CurrentOpInt = OSAllocZMemNoStats(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32));
		if (!pui32CurrentOpInt)
		{
			psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ServerSyncGetStatus_exit;
		}
	}

	if (psServerSyncGetStatusIN->ui32SyncCount != 0)
	{
		pui32NextOpInt = OSAllocZMemNoStats(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32));
		if (!pui32NextOpInt)
		{
			psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto ServerSyncGetStatus_exit;
		}
	}







	{
		IMG_UINT32 i;

		for (i=0;i<psServerSyncGetStatusIN->ui32SyncCount;i++)
		{
				{
					/* Look up the address from the handle */
					psServerSyncGetStatusOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt[i],
											hSyncHandleInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
											IMG_TRUE);
					if(psServerSyncGetStatusOUT->eError != PVRSRV_OK)
					{
						goto ServerSyncGetStatus_exit;
					}
				}
		}
	}

	psServerSyncGetStatusOUT->eError =
		PVRSRVServerSyncGetStatusKM(
					psServerSyncGetStatusIN->ui32SyncCount,
					psSyncHandleInt,
					pui32UIDInt,
					pui32FWAddrInt,
					pui32CurrentOpInt,
					pui32NextOpInt);



	if ( !OSAccessOK(PVR_VERIFY_WRITE, (void*) psServerSyncGetStatusOUT->pui32UID, (psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32)))
		|| (OSCopyToUser(NULL, psServerSyncGetStatusOUT->pui32UID, pui32UIDInt,
		(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32))) != PVRSRV_OK) )
	{
		psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto ServerSyncGetStatus_exit;
	}

	if ( !OSAccessOK(PVR_VERIFY_WRITE, (void*) psServerSyncGetStatusOUT->pui32FWAddr, (psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32)))
		|| (OSCopyToUser(NULL, psServerSyncGetStatusOUT->pui32FWAddr, pui32FWAddrInt,
		(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32))) != PVRSRV_OK) )
	{
		psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto ServerSyncGetStatus_exit;
	}

	if ( !OSAccessOK(PVR_VERIFY_WRITE, (void*) psServerSyncGetStatusOUT->pui32CurrentOp, (psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32)))
		|| (OSCopyToUser(NULL, psServerSyncGetStatusOUT->pui32CurrentOp, pui32CurrentOpInt,
		(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32))) != PVRSRV_OK) )
	{
		psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto ServerSyncGetStatus_exit;
	}

	if ( !OSAccessOK(PVR_VERIFY_WRITE, (void*) psServerSyncGetStatusOUT->pui32NextOp, (psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32)))
		|| (OSCopyToUser(NULL, psServerSyncGetStatusOUT->pui32NextOp, pui32NextOpInt,
		(psServerSyncGetStatusIN->ui32SyncCount * sizeof(IMG_UINT32))) != PVRSRV_OK) )
	{
		psServerSyncGetStatusOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto ServerSyncGetStatus_exit;
	}


ServerSyncGetStatus_exit:






	{
		IMG_UINT32 i;

		for (i=0;i<psServerSyncGetStatusIN->ui32SyncCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psSyncHandleInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncHandleInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
						}
				}
		}
	}

	if (psSyncHandleInt)
		OSFreeMemNoStats(psSyncHandleInt);
	if (hSyncHandleInt2)
		OSFreeMemNoStats(hSyncHandleInt2);
	if (pui32UIDInt)
		OSFreeMemNoStats(pui32UIDInt);
	if (pui32FWAddrInt)
		OSFreeMemNoStats(pui32FWAddrInt);
	if (pui32CurrentOpInt)
		OSFreeMemNoStats(pui32CurrentOpInt);
	if (pui32NextOpInt)
		OSFreeMemNoStats(pui32NextOpInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeSyncPrimOpCreate(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMOPCREATE *psSyncPrimOpCreateIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMOPCREATE *psSyncPrimOpCreateOUT,
					 CONNECTION_DATA *psConnection)
{
	SYNC_PRIMITIVE_BLOCK * *psBlockListInt = NULL;
	IMG_HANDLE *hBlockListInt2 = NULL;
	IMG_UINT32 *ui32SyncBlockIndexInt = NULL;
	IMG_UINT32 *ui32IndexInt = NULL;
	SERVER_SYNC_PRIMITIVE * *psServerSyncInt = NULL;
	IMG_HANDLE *hServerSyncInt2 = NULL;
	SERVER_OP_COOKIE * psServerCookieInt = NULL;




	if (psSyncPrimOpCreateIN->ui32SyncBlockCount != 0)
	{
		psBlockListInt = OSAllocZMemNoStats(psSyncPrimOpCreateIN->ui32SyncBlockCount * sizeof(SYNC_PRIMITIVE_BLOCK *));
		if (!psBlockListInt)
		{
			psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpCreate_exit;
		}
		hBlockListInt2 = OSAllocMemNoStats(psSyncPrimOpCreateIN->ui32SyncBlockCount * sizeof(IMG_HANDLE));
		if (!hBlockListInt2)
		{
			psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpCreate_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncPrimOpCreateIN->phBlockList, psSyncPrimOpCreateIN->ui32SyncBlockCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hBlockListInt2, psSyncPrimOpCreateIN->phBlockList,
				psSyncPrimOpCreateIN->ui32SyncBlockCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncPrimOpCreate_exit;
			}
	if (psSyncPrimOpCreateIN->ui32ClientSyncCount != 0)
	{
		ui32SyncBlockIndexInt = OSAllocZMemNoStats(psSyncPrimOpCreateIN->ui32ClientSyncCount * sizeof(IMG_UINT32));
		if (!ui32SyncBlockIndexInt)
		{
			psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpCreate_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncPrimOpCreateIN->pui32SyncBlockIndex, psSyncPrimOpCreateIN->ui32ClientSyncCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32SyncBlockIndexInt, psSyncPrimOpCreateIN->pui32SyncBlockIndex,
				psSyncPrimOpCreateIN->ui32ClientSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncPrimOpCreate_exit;
			}
	if (psSyncPrimOpCreateIN->ui32ClientSyncCount != 0)
	{
		ui32IndexInt = OSAllocZMemNoStats(psSyncPrimOpCreateIN->ui32ClientSyncCount * sizeof(IMG_UINT32));
		if (!ui32IndexInt)
		{
			psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpCreate_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncPrimOpCreateIN->pui32Index, psSyncPrimOpCreateIN->ui32ClientSyncCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32IndexInt, psSyncPrimOpCreateIN->pui32Index,
				psSyncPrimOpCreateIN->ui32ClientSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncPrimOpCreate_exit;
			}
	if (psSyncPrimOpCreateIN->ui32ServerSyncCount != 0)
	{
		psServerSyncInt = OSAllocZMemNoStats(psSyncPrimOpCreateIN->ui32ServerSyncCount * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psServerSyncInt)
		{
			psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpCreate_exit;
		}
		hServerSyncInt2 = OSAllocMemNoStats(psSyncPrimOpCreateIN->ui32ServerSyncCount * sizeof(IMG_HANDLE));
		if (!hServerSyncInt2)
		{
			psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpCreate_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncPrimOpCreateIN->phServerSync, psSyncPrimOpCreateIN->ui32ServerSyncCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hServerSyncInt2, psSyncPrimOpCreateIN->phServerSync,
				psSyncPrimOpCreateIN->ui32ServerSyncCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psSyncPrimOpCreateOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncPrimOpCreate_exit;
			}






	{
		IMG_UINT32 i;

		for (i=0;i<psSyncPrimOpCreateIN->ui32SyncBlockCount;i++)
		{
				{
					/* Look up the address from the handle */
					psSyncPrimOpCreateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psBlockListInt[i],
											hBlockListInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psSyncPrimOpCreateOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimOpCreate_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psSyncPrimOpCreateIN->ui32ServerSyncCount;i++)
		{
				{
					/* Look up the address from the handle */
					psSyncPrimOpCreateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServerSyncInt[i],
											hServerSyncInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
											IMG_TRUE);
					if(psSyncPrimOpCreateOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimOpCreate_exit;
					}
				}
		}
	}

	psSyncPrimOpCreateOUT->eError =
		PVRSRVSyncPrimOpCreateKM(
					psSyncPrimOpCreateIN->ui32SyncBlockCount,
					psBlockListInt,
					psSyncPrimOpCreateIN->ui32ClientSyncCount,
					ui32SyncBlockIndexInt,
					ui32IndexInt,
					psSyncPrimOpCreateIN->ui32ServerSyncCount,
					psServerSyncInt,
					&psServerCookieInt);
	/* Exit early if bridged call fails */
	if(psSyncPrimOpCreateOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimOpCreate_exit;
	}






	psSyncPrimOpCreateOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psSyncPrimOpCreateOUT->hServerCookie,
							(void *) psServerCookieInt,
							PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PVRSRVSyncPrimOpDestroyKM);
	if (psSyncPrimOpCreateOUT->eError != PVRSRV_OK)
	{
		goto SyncPrimOpCreate_exit;
	}




SyncPrimOpCreate_exit:






	{
		IMG_UINT32 i;

		for (i=0;i<psSyncPrimOpCreateIN->ui32SyncBlockCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psBlockListInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hBlockListInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psSyncPrimOpCreateIN->ui32ServerSyncCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psServerSyncInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hServerSyncInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
						}
				}
		}
	}

	if (psSyncPrimOpCreateOUT->eError != PVRSRV_OK)
	{
		if (psServerCookieInt)
		{
			PVRSRVSyncPrimOpDestroyKM(psServerCookieInt);
		}
	}

	if (psBlockListInt)
		OSFreeMemNoStats(psBlockListInt);
	if (hBlockListInt2)
		OSFreeMemNoStats(hBlockListInt2);
	if (ui32SyncBlockIndexInt)
		OSFreeMemNoStats(ui32SyncBlockIndexInt);
	if (ui32IndexInt)
		OSFreeMemNoStats(ui32IndexInt);
	if (psServerSyncInt)
		OSFreeMemNoStats(psServerSyncInt);
	if (hServerSyncInt2)
		OSFreeMemNoStats(hServerSyncInt2);

	return 0;
}


static IMG_INT
PVRSRVBridgeSyncPrimOpTake(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMOPTAKE *psSyncPrimOpTakeIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMOPTAKE *psSyncPrimOpTakeOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hServerCookie = psSyncPrimOpTakeIN->hServerCookie;
	SERVER_OP_COOKIE * psServerCookieInt = NULL;
	IMG_UINT32 *ui32FlagsInt = NULL;
	IMG_UINT32 *ui32FenceValueInt = NULL;
	IMG_UINT32 *ui32UpdateValueInt = NULL;
	IMG_UINT32 *ui32ServerFlagsInt = NULL;




	if (psSyncPrimOpTakeIN->ui32ClientSyncCount != 0)
	{
		ui32FlagsInt = OSAllocZMemNoStats(psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32));
		if (!ui32FlagsInt)
		{
			psSyncPrimOpTakeOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpTake_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncPrimOpTakeIN->pui32Flags, psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32FlagsInt, psSyncPrimOpTakeIN->pui32Flags,
				psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psSyncPrimOpTakeOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncPrimOpTake_exit;
			}
	if (psSyncPrimOpTakeIN->ui32ClientSyncCount != 0)
	{
		ui32FenceValueInt = OSAllocZMemNoStats(psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32));
		if (!ui32FenceValueInt)
		{
			psSyncPrimOpTakeOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpTake_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncPrimOpTakeIN->pui32FenceValue, psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32FenceValueInt, psSyncPrimOpTakeIN->pui32FenceValue,
				psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psSyncPrimOpTakeOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncPrimOpTake_exit;
			}
	if (psSyncPrimOpTakeIN->ui32ClientSyncCount != 0)
	{
		ui32UpdateValueInt = OSAllocZMemNoStats(psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32));
		if (!ui32UpdateValueInt)
		{
			psSyncPrimOpTakeOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpTake_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncPrimOpTakeIN->pui32UpdateValue, psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32UpdateValueInt, psSyncPrimOpTakeIN->pui32UpdateValue,
				psSyncPrimOpTakeIN->ui32ClientSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psSyncPrimOpTakeOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncPrimOpTake_exit;
			}
	if (psSyncPrimOpTakeIN->ui32ServerSyncCount != 0)
	{
		ui32ServerFlagsInt = OSAllocZMemNoStats(psSyncPrimOpTakeIN->ui32ServerSyncCount * sizeof(IMG_UINT32));
		if (!ui32ServerFlagsInt)
		{
			psSyncPrimOpTakeOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto SyncPrimOpTake_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psSyncPrimOpTakeIN->pui32ServerFlags, psSyncPrimOpTakeIN->ui32ServerSyncCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ServerFlagsInt, psSyncPrimOpTakeIN->pui32ServerFlags,
				psSyncPrimOpTakeIN->ui32ServerSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psSyncPrimOpTakeOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto SyncPrimOpTake_exit;
			}






				{
					/* Look up the address from the handle */
					psSyncPrimOpTakeOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServerCookieInt,
											hServerCookie,
											PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE,
											IMG_TRUE);
					if(psSyncPrimOpTakeOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimOpTake_exit;
					}
				}

	psSyncPrimOpTakeOUT->eError =
		PVRSRVSyncPrimOpTakeKM(
					psServerCookieInt,
					psSyncPrimOpTakeIN->ui32ClientSyncCount,
					ui32FlagsInt,
					ui32FenceValueInt,
					ui32UpdateValueInt,
					psSyncPrimOpTakeIN->ui32ServerSyncCount,
					ui32ServerFlagsInt);




SyncPrimOpTake_exit:






				{
					/* Unreference the previously looked up handle */
						if(psServerCookieInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hServerCookie,
											PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE);
						}
				}

	if (ui32FlagsInt)
		OSFreeMemNoStats(ui32FlagsInt);
	if (ui32FenceValueInt)
		OSFreeMemNoStats(ui32FenceValueInt);
	if (ui32UpdateValueInt)
		OSFreeMemNoStats(ui32UpdateValueInt);
	if (ui32ServerFlagsInt)
		OSFreeMemNoStats(ui32ServerFlagsInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeSyncPrimOpReady(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMOPREADY *psSyncPrimOpReadyIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMOPREADY *psSyncPrimOpReadyOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hServerCookie = psSyncPrimOpReadyIN->hServerCookie;
	SERVER_OP_COOKIE * psServerCookieInt = NULL;










				{
					/* Look up the address from the handle */
					psSyncPrimOpReadyOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServerCookieInt,
											hServerCookie,
											PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE,
											IMG_TRUE);
					if(psSyncPrimOpReadyOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimOpReady_exit;
					}
				}

	psSyncPrimOpReadyOUT->eError =
		PVRSRVSyncPrimOpReadyKM(
					psServerCookieInt,
					&psSyncPrimOpReadyOUT->bReady);




SyncPrimOpReady_exit:






				{
					/* Unreference the previously looked up handle */
						if(psServerCookieInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hServerCookie,
											PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeSyncPrimOpComplete(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMOPCOMPLETE *psSyncPrimOpCompleteIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMOPCOMPLETE *psSyncPrimOpCompleteOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hServerCookie = psSyncPrimOpCompleteIN->hServerCookie;
	SERVER_OP_COOKIE * psServerCookieInt = NULL;










				{
					/* Look up the address from the handle */
					psSyncPrimOpCompleteOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServerCookieInt,
											hServerCookie,
											PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE,
											IMG_TRUE);
					if(psSyncPrimOpCompleteOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimOpComplete_exit;
					}
				}

	psSyncPrimOpCompleteOUT->eError =
		PVRSRVSyncPrimOpCompleteKM(
					psServerCookieInt);




SyncPrimOpComplete_exit:






				{
					/* Unreference the previously looked up handle */
						if(psServerCookieInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hServerCookie,
											PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE);
						}
				}


	return 0;
}


static IMG_INT
PVRSRVBridgeSyncPrimOpDestroy(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMOPDESTROY *psSyncPrimOpDestroyIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMOPDESTROY *psSyncPrimOpDestroyOUT,
					 CONNECTION_DATA *psConnection)
{












	psSyncPrimOpDestroyOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psSyncPrimOpDestroyIN->hServerCookie,
					PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE);
	if ((psSyncPrimOpDestroyOUT->eError != PVRSRV_OK) && (psSyncPrimOpDestroyOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto SyncPrimOpDestroy_exit;
	}




SyncPrimOpDestroy_exit:



	return 0;
}


#if defined(PDUMP)
static IMG_INT
PVRSRVBridgeSyncPrimPDump(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMPDUMP *psSyncPrimPDumpIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMPDUMP *psSyncPrimPDumpOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hSyncHandle = psSyncPrimPDumpIN->hSyncHandle;
	SYNC_PRIMITIVE_BLOCK * psSyncHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psSyncPrimPDumpOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psSyncPrimPDumpOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimPDump_exit;
					}
				}

	psSyncPrimPDumpOUT->eError =
		PVRSRVSyncPrimPDumpKM(
					psSyncHandleInt,
					psSyncPrimPDumpIN->ui32Offset);




SyncPrimPDump_exit:






				{
					/* Unreference the previously looked up handle */
						if(psSyncHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}


	return 0;
}

#else
#define PVRSRVBridgeSyncPrimPDump NULL
#endif

#if defined(PDUMP)
static IMG_INT
PVRSRVBridgeSyncPrimPDumpValue(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMPDUMPVALUE *psSyncPrimPDumpValueIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMPDUMPVALUE *psSyncPrimPDumpValueOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hSyncHandle = psSyncPrimPDumpValueIN->hSyncHandle;
	SYNC_PRIMITIVE_BLOCK * psSyncHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psSyncPrimPDumpValueOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psSyncPrimPDumpValueOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimPDumpValue_exit;
					}
				}

	psSyncPrimPDumpValueOUT->eError =
		PVRSRVSyncPrimPDumpValueKM(
					psSyncHandleInt,
					psSyncPrimPDumpValueIN->ui32Offset,
					psSyncPrimPDumpValueIN->ui32Value);




SyncPrimPDumpValue_exit:






				{
					/* Unreference the previously looked up handle */
						if(psSyncHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}


	return 0;
}

#else
#define PVRSRVBridgeSyncPrimPDumpValue NULL
#endif

#if defined(PDUMP)
static IMG_INT
PVRSRVBridgeSyncPrimPDumpPol(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMPDUMPPOL *psSyncPrimPDumpPolIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMPDUMPPOL *psSyncPrimPDumpPolOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hSyncHandle = psSyncPrimPDumpPolIN->hSyncHandle;
	SYNC_PRIMITIVE_BLOCK * psSyncHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psSyncPrimPDumpPolOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psSyncPrimPDumpPolOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimPDumpPol_exit;
					}
				}

	psSyncPrimPDumpPolOUT->eError =
		PVRSRVSyncPrimPDumpPolKM(
					psSyncHandleInt,
					psSyncPrimPDumpPolIN->ui32Offset,
					psSyncPrimPDumpPolIN->ui32Value,
					psSyncPrimPDumpPolIN->ui32Mask,
					psSyncPrimPDumpPolIN->eOperator,
					psSyncPrimPDumpPolIN->uiPDumpFlags);




SyncPrimPDumpPol_exit:






				{
					/* Unreference the previously looked up handle */
						if(psSyncHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}


	return 0;
}

#else
#define PVRSRVBridgeSyncPrimPDumpPol NULL
#endif

#if defined(PDUMP)
static IMG_INT
PVRSRVBridgeSyncPrimOpPDumpPol(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMOPPDUMPPOL *psSyncPrimOpPDumpPolIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMOPPDUMPPOL *psSyncPrimOpPDumpPolOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hServerCookie = psSyncPrimOpPDumpPolIN->hServerCookie;
	SERVER_OP_COOKIE * psServerCookieInt = NULL;










				{
					/* Look up the address from the handle */
					psSyncPrimOpPDumpPolOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServerCookieInt,
											hServerCookie,
											PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE,
											IMG_TRUE);
					if(psSyncPrimOpPDumpPolOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimOpPDumpPol_exit;
					}
				}

	psSyncPrimOpPDumpPolOUT->eError =
		PVRSRVSyncPrimOpPDumpPolKM(
					psServerCookieInt,
					psSyncPrimOpPDumpPolIN->eOperator,
					psSyncPrimOpPDumpPolIN->uiPDumpFlags);




SyncPrimOpPDumpPol_exit:






				{
					/* Unreference the previously looked up handle */
						if(psServerCookieInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hServerCookie,
											PVRSRV_HANDLE_TYPE_SERVER_OP_COOKIE);
						}
				}


	return 0;
}

#else
#define PVRSRVBridgeSyncPrimOpPDumpPol NULL
#endif

#if defined(PDUMP)
static IMG_INT
PVRSRVBridgeSyncPrimPDumpCBP(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCPRIMPDUMPCBP *psSyncPrimPDumpCBPIN,
					  PVRSRV_BRIDGE_OUT_SYNCPRIMPDUMPCBP *psSyncPrimPDumpCBPOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hSyncHandle = psSyncPrimPDumpCBPIN->hSyncHandle;
	SYNC_PRIMITIVE_BLOCK * psSyncHandleInt = NULL;










				{
					/* Look up the address from the handle */
					psSyncPrimPDumpCBPOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncHandleInt,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psSyncPrimPDumpCBPOUT->eError != PVRSRV_OK)
					{
						goto SyncPrimPDumpCBP_exit;
					}
				}

	psSyncPrimPDumpCBPOUT->eError =
		PVRSRVSyncPrimPDumpCBPKM(
					psSyncHandleInt,
					psSyncPrimPDumpCBPIN->ui32Offset,
					psSyncPrimPDumpCBPIN->uiWriteOffset,
					psSyncPrimPDumpCBPIN->uiPacketSize,
					psSyncPrimPDumpCBPIN->uiBufferSize);




SyncPrimPDumpCBP_exit:






				{
					/* Unreference the previously looked up handle */
						if(psSyncHandleInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hSyncHandle,
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}


	return 0;
}

#else
#define PVRSRVBridgeSyncPrimPDumpCBP NULL
#endif

#if !defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
static IMG_INT
PVRSRVBridgeSyncFreeEvent(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_SYNCFREEEVENT *psSyncFreeEventIN,
					  PVRSRV_BRIDGE_OUT_SYNCFREEEVENT *psSyncFreeEventOUT,
					 CONNECTION_DATA *psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psConnection);





	psSyncFreeEventOUT->eError =
		PVRSRVSyncFreeEventKM(
					psSyncFreeEventIN->ui32FWAddr);







	return 0;
}

#else
#define PVRSRVBridgeSyncFreeEvent NULL
#endif



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */

static IMG_BOOL bUseLock = IMG_TRUE;

PVRSRV_ERROR InitSYNCBridge(void);
PVRSRV_ERROR DeinitSYNCBridge(void);

/*
 * Register all SYNC functions with services
 */
PVRSRV_ERROR InitSYNCBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_ALLOCSYNCPRIMITIVEBLOCK, PVRSRVBridgeAllocSyncPrimitiveBlock,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_FREESYNCPRIMITIVEBLOCK, PVRSRVBridgeFreeSyncPrimitiveBlock,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMSET, PVRSRVBridgeSyncPrimSet,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SERVERSYNCPRIMSET, PVRSRVBridgeServerSyncPrimSet,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCRECORDREMOVEBYHANDLE, PVRSRVBridgeSyncRecordRemoveByHandle,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCRECORDADD, PVRSRVBridgeSyncRecordAdd,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCALLOCEVENT, PVRSRVBridgeSyncAllocEvent,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SERVERSYNCALLOC, PVRSRVBridgeServerSyncAlloc,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SERVERSYNCFREE, PVRSRVBridgeServerSyncFree,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SERVERSYNCQUEUEHWOP, PVRSRVBridgeServerSyncQueueHWOp,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SERVERSYNCGETSTATUS, PVRSRVBridgeServerSyncGetStatus,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMOPCREATE, PVRSRVBridgeSyncPrimOpCreate,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMOPTAKE, PVRSRVBridgeSyncPrimOpTake,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMOPREADY, PVRSRVBridgeSyncPrimOpReady,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMOPCOMPLETE, PVRSRVBridgeSyncPrimOpComplete,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMOPDESTROY, PVRSRVBridgeSyncPrimOpDestroy,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMPDUMP, PVRSRVBridgeSyncPrimPDump,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMPDUMPVALUE, PVRSRVBridgeSyncPrimPDumpValue,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMPDUMPPOL, PVRSRVBridgeSyncPrimPDumpPol,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMOPPDUMPPOL, PVRSRVBridgeSyncPrimOpPDumpPol,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCPRIMPDUMPCBP, PVRSRVBridgeSyncPrimPDumpCBP,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_SYNC, PVRSRV_BRIDGE_SYNC_SYNCFREEEVENT, PVRSRVBridgeSyncFreeEvent,
					NULL, bUseLock);


	return PVRSRV_OK;
}

/*
 * Unregister all sync functions with services
 */
PVRSRV_ERROR DeinitSYNCBridge(void)
{
	return PVRSRV_OK;
}
