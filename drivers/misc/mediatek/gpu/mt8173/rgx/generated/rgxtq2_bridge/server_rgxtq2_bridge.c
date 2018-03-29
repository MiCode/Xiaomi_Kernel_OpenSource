/*************************************************************************/ /*!
@File
@Title          Server bridge for rgxtq2
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxtq2
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

#include "rgxtdmtransfer.h"


#include "common_rgxtq2_bridge.h"

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
PVRSRVBridgeRGXTDMCreateTransferContext(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXTDMCREATETRANSFERCONTEXT *psRGXTDMCreateTransferContextIN,
					  PVRSRV_BRIDGE_OUT_RGXTDMCREATETRANSFERCONTEXT *psRGXTDMCreateTransferContextOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_BYTE *psFrameworkCmdInt = NULL;
	IMG_HANDLE hPrivData = psRGXTDMCreateTransferContextIN->hPrivData;
	IMG_HANDLE hPrivDataInt = NULL;
	RGX_SERVER_TQ_TDM_CONTEXT * psTransferContextInt = NULL;




	if (psRGXTDMCreateTransferContextIN->ui32FrameworkCmdize != 0)
	{
		psFrameworkCmdInt = OSAllocZMemNoStats(psRGXTDMCreateTransferContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE));
		if (!psFrameworkCmdInt)
		{
			psRGXTDMCreateTransferContextOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMCreateTransferContext_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMCreateTransferContextIN->psFrameworkCmd, psRGXTDMCreateTransferContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, psFrameworkCmdInt, psRGXTDMCreateTransferContextIN->psFrameworkCmd,
				psRGXTDMCreateTransferContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXTDMCreateTransferContextOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMCreateTransferContext_exit;
			}






				{
					/* Look up the address from the handle */
					psRGXTDMCreateTransferContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &hPrivDataInt,
											hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
											IMG_TRUE);
					if(psRGXTDMCreateTransferContextOUT->eError != PVRSRV_OK)
					{
						goto RGXTDMCreateTransferContext_exit;
					}
				}

	psRGXTDMCreateTransferContextOUT->eError =
		PVRSRVRGXTDMCreateTransferContextKM(psConnection, OSGetDevData(psConnection),
					psRGXTDMCreateTransferContextIN->ui32Priority,
					psRGXTDMCreateTransferContextIN->sMCUFenceAddr,
					psRGXTDMCreateTransferContextIN->ui32FrameworkCmdize,
					psFrameworkCmdInt,
					hPrivDataInt,
					&psTransferContextInt);
	/* Exit early if bridged call fails */
	if(psRGXTDMCreateTransferContextOUT->eError != PVRSRV_OK)
	{
		goto RGXTDMCreateTransferContext_exit;
	}






	psRGXTDMCreateTransferContextOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRGXTDMCreateTransferContextOUT->hTransferContext,
							(void *) psTransferContextInt,
							PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PVRSRVRGXTDMDestroyTransferContextKM);
	if (psRGXTDMCreateTransferContextOUT->eError != PVRSRV_OK)
	{
		goto RGXTDMCreateTransferContext_exit;
	}




RGXTDMCreateTransferContext_exit:






				{
					/* Unreference the previously looked up handle */
						if(hPrivDataInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
						}
				}

	if (psRGXTDMCreateTransferContextOUT->eError != PVRSRV_OK)
	{
		if (psTransferContextInt)
		{
			PVRSRVRGXTDMDestroyTransferContextKM(psTransferContextInt);
		}
	}

	if (psFrameworkCmdInt)
		OSFreeMemNoStats(psFrameworkCmdInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeRGXTDMDestroyTransferContext(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXTDMDESTROYTRANSFERCONTEXT *psRGXTDMDestroyTransferContextIN,
					  PVRSRV_BRIDGE_OUT_RGXTDMDESTROYTRANSFERCONTEXT *psRGXTDMDestroyTransferContextOUT,
					 CONNECTION_DATA *psConnection)
{












	psRGXTDMDestroyTransferContextOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXTDMDestroyTransferContextIN->hTransferContext,
					PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
	if ((psRGXTDMDestroyTransferContextOUT->eError != PVRSRV_OK) && (psRGXTDMDestroyTransferContextOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RGXTDMDestroyTransferContext_exit;
	}




RGXTDMDestroyTransferContext_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRGXTDMSubmitTransfer(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXTDMSUBMITTRANSFER *psRGXTDMSubmitTransferIN,
					  PVRSRV_BRIDGE_OUT_RGXTDMSUBMITTRANSFER *psRGXTDMSubmitTransferOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hTransferContext = psRGXTDMSubmitTransferIN->hTransferContext;
	RGX_SERVER_TQ_TDM_CONTEXT * psTransferContextInt = NULL;
	SYNC_PRIMITIVE_BLOCK * *psFenceUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE *hFenceUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32FenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32FenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK * *psUpdateUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE *hUpdateUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32UpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32UpdateValueInt = NULL;
	IMG_UINT32 *ui32ServerSyncFlagsInt = NULL;
	SERVER_SYNC_PRIMITIVE * *psServerSyncInt = NULL;
	IMG_HANDLE *hServerSyncInt2 = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_UINT8 *ui8FWCommandInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR * *psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;




	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount != 0)
	{
		psFenceUFOSyncPrimBlockInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(SYNC_PRIMITIVE_BLOCK *));
		if (!psFenceUFOSyncPrimBlockInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
		hFenceUFOSyncPrimBlockInt2 = OSAllocMemNoStats(psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_HANDLE));
		if (!hFenceUFOSyncPrimBlockInt2)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->phFenceUFOSyncPrimBlock, psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hFenceUFOSyncPrimBlockInt2, psRGXTDMSubmitTransferIN->phFenceUFOSyncPrimBlock,
				psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount != 0)
	{
		ui32FenceSyncOffsetInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_UINT32));
		if (!ui32FenceSyncOffsetInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->pui32FenceSyncOffset, psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32FenceSyncOffsetInt, psRGXTDMSubmitTransferIN->pui32FenceSyncOffset,
				psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount != 0)
	{
		ui32FenceValueInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_UINT32));
		if (!ui32FenceValueInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->pui32FenceValue, psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32FenceValueInt, psRGXTDMSubmitTransferIN->pui32FenceValue,
				psRGXTDMSubmitTransferIN->ui32ClientFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount != 0)
	{
		psUpdateUFOSyncPrimBlockInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(SYNC_PRIMITIVE_BLOCK *));
		if (!psUpdateUFOSyncPrimBlockInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
		hUpdateUFOSyncPrimBlockInt2 = OSAllocMemNoStats(psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_HANDLE));
		if (!hUpdateUFOSyncPrimBlockInt2)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->phUpdateUFOSyncPrimBlock, psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hUpdateUFOSyncPrimBlockInt2, psRGXTDMSubmitTransferIN->phUpdateUFOSyncPrimBlock,
				psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount != 0)
	{
		ui32UpdateSyncOffsetInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_UINT32));
		if (!ui32UpdateSyncOffsetInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->pui32UpdateSyncOffset, psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32UpdateSyncOffsetInt, psRGXTDMSubmitTransferIN->pui32UpdateSyncOffset,
				psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount != 0)
	{
		ui32UpdateValueInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_UINT32));
		if (!ui32UpdateValueInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->pui32UpdateValue, psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32UpdateValueInt, psRGXTDMSubmitTransferIN->pui32UpdateValue,
				psRGXTDMSubmitTransferIN->ui32ClientUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32ServerSyncCount != 0)
	{
		ui32ServerSyncFlagsInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(IMG_UINT32));
		if (!ui32ServerSyncFlagsInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->pui32ServerSyncFlags, psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ServerSyncFlagsInt, psRGXTDMSubmitTransferIN->pui32ServerSyncFlags,
				psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32ServerSyncCount != 0)
	{
		psServerSyncInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psServerSyncInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
		hServerSyncInt2 = OSAllocMemNoStats(psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(IMG_HANDLE));
		if (!hServerSyncInt2)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->phServerSync, psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hServerSyncInt2, psRGXTDMSubmitTransferIN->phServerSync,
				psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	
	{
		uiUpdateFenceNameInt = OSAllocZMemNoStats(32 * sizeof(IMG_CHAR));
		if (!uiUpdateFenceNameInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->puiUpdateFenceName, 32 * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiUpdateFenceNameInt, psRGXTDMSubmitTransferIN->puiUpdateFenceName,
				32 * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32CommandSize != 0)
	{
		ui8FWCommandInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32CommandSize * sizeof(IMG_UINT8));
		if (!ui8FWCommandInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->pui8FWCommand, psRGXTDMSubmitTransferIN->ui32CommandSize * sizeof(IMG_UINT8))
				|| (OSCopyFromUser(NULL, ui8FWCommandInt, psRGXTDMSubmitTransferIN->pui8FWCommand,
				psRGXTDMSubmitTransferIN->ui32CommandSize * sizeof(IMG_UINT8)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32));
		if (!ui32SyncPMRFlagsInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->pui32SyncPMRFlags, psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32SyncPMRFlagsInt, psRGXTDMSubmitTransferIN->pui32SyncPMRFlags,
				psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}
	if (psRGXTDMSubmitTransferIN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt = OSAllocZMemNoStats(psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(PMR *));
		if (!psSyncPMRsInt)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
		hSyncPMRsInt2 = OSAllocMemNoStats(psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE));
		if (!hSyncPMRsInt2)
		{
			psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXTDMSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXTDMSubmitTransferIN->phSyncPMRs, psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hSyncPMRsInt2, psRGXTDMSubmitTransferIN->phSyncPMRs,
				psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXTDMSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXTDMSubmitTransfer_exit;
			}






				{
					/* Look up the address from the handle */
					psRGXTDMSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psTransferContextInt,
											hTransferContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
											IMG_TRUE);
					if(psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXTDMSubmitTransfer_exit;
					}
				}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXTDMSubmitTransferIN->ui32ClientFenceCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXTDMSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psFenceUFOSyncPrimBlockInt[i],
											hFenceUFOSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXTDMSubmitTransfer_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXTDMSubmitTransferIN->ui32ClientUpdateCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXTDMSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psUpdateUFOSyncPrimBlockInt[i],
											hUpdateUFOSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXTDMSubmitTransfer_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXTDMSubmitTransferIN->ui32ServerSyncCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXTDMSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServerSyncInt[i],
											hServerSyncInt2[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
											IMG_TRUE);
					if(psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXTDMSubmitTransfer_exit;
					}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXTDMSubmitTransferIN->ui32SyncPMRCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXTDMSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncPMRsInt[i],
											hSyncPMRsInt2[i],
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXTDMSubmitTransfer_exit;
					}
				}
		}
	}

	psRGXTDMSubmitTransferOUT->eError =
		PVRSRVRGXTDMSubmitTransferKM(
					psTransferContextInt,
					psRGXTDMSubmitTransferIN->bPDumpContinuous,
					psRGXTDMSubmitTransferIN->ui32ClientCacheOpSeqNum,
					psRGXTDMSubmitTransferIN->ui32ClientFenceCount,
					psFenceUFOSyncPrimBlockInt,
					ui32FenceSyncOffsetInt,
					ui32FenceValueInt,
					psRGXTDMSubmitTransferIN->ui32ClientUpdateCount,
					psUpdateUFOSyncPrimBlockInt,
					ui32UpdateSyncOffsetInt,
					ui32UpdateValueInt,
					psRGXTDMSubmitTransferIN->ui32ServerSyncCount,
					ui32ServerSyncFlagsInt,
					psServerSyncInt,
					psRGXTDMSubmitTransferIN->i32CheckFenceFD,
					psRGXTDMSubmitTransferIN->i32UpdateTimelineFD,
					&psRGXTDMSubmitTransferOUT->i32UpdateFenceFD,
					uiUpdateFenceNameInt,
					psRGXTDMSubmitTransferIN->ui32CommandSize,
					ui8FWCommandInt,
					psRGXTDMSubmitTransferIN->ui32ExternalJobReference,
					psRGXTDMSubmitTransferIN->ui32SyncPMRCount,
					ui32SyncPMRFlagsInt,
					psSyncPMRsInt);




RGXTDMSubmitTransfer_exit:






				{
					/* Unreference the previously looked up handle */
						if(psTransferContextInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hTransferContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
						}
				}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXTDMSubmitTransferIN->ui32ClientFenceCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psFenceUFOSyncPrimBlockInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hFenceUFOSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXTDMSubmitTransferIN->ui32ClientUpdateCount;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psUpdateUFOSyncPrimBlockInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hUpdateUFOSyncPrimBlockInt2[i],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXTDMSubmitTransferIN->ui32ServerSyncCount;i++)
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





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXTDMSubmitTransferIN->ui32SyncPMRCount;i++)
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

	if (psFenceUFOSyncPrimBlockInt)
		OSFreeMemNoStats(psFenceUFOSyncPrimBlockInt);
	if (hFenceUFOSyncPrimBlockInt2)
		OSFreeMemNoStats(hFenceUFOSyncPrimBlockInt2);
	if (ui32FenceSyncOffsetInt)
		OSFreeMemNoStats(ui32FenceSyncOffsetInt);
	if (ui32FenceValueInt)
		OSFreeMemNoStats(ui32FenceValueInt);
	if (psUpdateUFOSyncPrimBlockInt)
		OSFreeMemNoStats(psUpdateUFOSyncPrimBlockInt);
	if (hUpdateUFOSyncPrimBlockInt2)
		OSFreeMemNoStats(hUpdateUFOSyncPrimBlockInt2);
	if (ui32UpdateSyncOffsetInt)
		OSFreeMemNoStats(ui32UpdateSyncOffsetInt);
	if (ui32UpdateValueInt)
		OSFreeMemNoStats(ui32UpdateValueInt);
	if (ui32ServerSyncFlagsInt)
		OSFreeMemNoStats(ui32ServerSyncFlagsInt);
	if (psServerSyncInt)
		OSFreeMemNoStats(psServerSyncInt);
	if (hServerSyncInt2)
		OSFreeMemNoStats(hServerSyncInt2);
	if (uiUpdateFenceNameInt)
		OSFreeMemNoStats(uiUpdateFenceNameInt);
	if (ui8FWCommandInt)
		OSFreeMemNoStats(ui8FWCommandInt);
	if (ui32SyncPMRFlagsInt)
		OSFreeMemNoStats(ui32SyncPMRFlagsInt);
	if (psSyncPMRsInt)
		OSFreeMemNoStats(psSyncPMRsInt);
	if (hSyncPMRsInt2)
		OSFreeMemNoStats(hSyncPMRsInt2);

	return 0;
}


static IMG_INT
PVRSRVBridgeRGXTDMSetTransferContextPriority(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXTDMSETTRANSFERCONTEXTPRIORITY *psRGXTDMSetTransferContextPriorityIN,
					  PVRSRV_BRIDGE_OUT_RGXTDMSETTRANSFERCONTEXTPRIORITY *psRGXTDMSetTransferContextPriorityOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hTransferContext = psRGXTDMSetTransferContextPriorityIN->hTransferContext;
	RGX_SERVER_TQ_TDM_CONTEXT * psTransferContextInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXTDMSetTransferContextPriorityOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psTransferContextInt,
											hTransferContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
											IMG_TRUE);
					if(psRGXTDMSetTransferContextPriorityOUT->eError != PVRSRV_OK)
					{
						goto RGXTDMSetTransferContextPriority_exit;
					}
				}

	psRGXTDMSetTransferContextPriorityOUT->eError =
		PVRSRVRGXTDMSetTransferContextPriorityKM(psConnection, OSGetDevData(psConnection),
					psTransferContextInt,
					psRGXTDMSetTransferContextPriorityIN->ui32Priority);




RGXTDMSetTransferContextPriority_exit:






				{
					/* Unreference the previously looked up handle */
						if(psTransferContextInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hTransferContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
						}
				}


	return 0;
}




/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */

static IMG_BOOL bUseLock = IMG_TRUE;

PVRSRV_ERROR InitRGXTQ2Bridge(void);
PVRSRV_ERROR DeinitRGXTQ2Bridge(void);

/*
 * Register all RGXTQ2 functions with services
 */
PVRSRV_ERROR InitRGXTQ2Bridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2, PVRSRV_BRIDGE_RGXTQ2_RGXTDMCREATETRANSFERCONTEXT, PVRSRVBridgeRGXTDMCreateTransferContext,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2, PVRSRV_BRIDGE_RGXTQ2_RGXTDMDESTROYTRANSFERCONTEXT, PVRSRVBridgeRGXTDMDestroyTransferContext,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2, PVRSRV_BRIDGE_RGXTQ2_RGXTDMSUBMITTRANSFER, PVRSRVBridgeRGXTDMSubmitTransfer,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2, PVRSRV_BRIDGE_RGXTQ2_RGXTDMSETTRANSFERCONTEXTPRIORITY, PVRSRVBridgeRGXTDMSetTransferContextPriority,
					NULL, bUseLock);


	return PVRSRV_OK;
}

/*
 * Unregister all rgxtq2 functions with services
 */
PVRSRV_ERROR DeinitRGXTQ2Bridge(void)
{
	return PVRSRV_OK;
}
