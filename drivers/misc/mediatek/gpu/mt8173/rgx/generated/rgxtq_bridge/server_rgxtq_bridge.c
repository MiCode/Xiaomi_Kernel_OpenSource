/*************************************************************************/ /*!
@File
@Title          Server bridge for rgxtq
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxtq
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

#include "rgxtransfer.h"


#include "common_rgxtq_bridge.h"

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
PVRSRVBridgeRGXCreateTransferContext(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXCREATETRANSFERCONTEXT *psRGXCreateTransferContextIN,
					  PVRSRV_BRIDGE_OUT_RGXCREATETRANSFERCONTEXT *psRGXCreateTransferContextOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_BYTE *psFrameworkCmdInt = NULL;
	IMG_HANDLE hPrivData = psRGXCreateTransferContextIN->hPrivData;
	IMG_HANDLE hPrivDataInt = NULL;
	RGX_SERVER_TQ_CONTEXT * psTransferContextInt = NULL;




	if (psRGXCreateTransferContextIN->ui32FrameworkCmdize != 0)
	{
		psFrameworkCmdInt = OSAllocZMemNoStats(psRGXCreateTransferContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE));
		if (!psFrameworkCmdInt)
		{
			psRGXCreateTransferContextOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXCreateTransferContext_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXCreateTransferContextIN->psFrameworkCmd, psRGXCreateTransferContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, psFrameworkCmdInt, psRGXCreateTransferContextIN->psFrameworkCmd,
				psRGXCreateTransferContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXCreateTransferContextOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXCreateTransferContext_exit;
			}






				{
					/* Look up the address from the handle */
					psRGXCreateTransferContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &hPrivDataInt,
											hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
											IMG_TRUE);
					if(psRGXCreateTransferContextOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateTransferContext_exit;
					}
				}

	psRGXCreateTransferContextOUT->eError =
		PVRSRVRGXCreateTransferContextKM(psConnection, OSGetDevData(psConnection),
					psRGXCreateTransferContextIN->ui32Priority,
					psRGXCreateTransferContextIN->sMCUFenceAddr,
					psRGXCreateTransferContextIN->ui32FrameworkCmdize,
					psFrameworkCmdInt,
					hPrivDataInt,
					&psTransferContextInt);
	/* Exit early if bridged call fails */
	if(psRGXCreateTransferContextOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateTransferContext_exit;
	}






	psRGXCreateTransferContextOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,

							&psRGXCreateTransferContextOUT->hTransferContext,
							(void *) psTransferContextInt,
							PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							,(PFN_HANDLE_RELEASE)&PVRSRVRGXDestroyTransferContextKM);
	if (psRGXCreateTransferContextOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateTransferContext_exit;
	}




RGXCreateTransferContext_exit:






				{
					/* Unreference the previously looked up handle */
						if(hPrivDataInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
						}
				}

	if (psRGXCreateTransferContextOUT->eError != PVRSRV_OK)
	{
		if (psTransferContextInt)
		{
			PVRSRVRGXDestroyTransferContextKM(psTransferContextInt);
		}
	}

	if (psFrameworkCmdInt)
		OSFreeMemNoStats(psFrameworkCmdInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeRGXDestroyTransferContext(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXDESTROYTRANSFERCONTEXT *psRGXDestroyTransferContextIN,
					  PVRSRV_BRIDGE_OUT_RGXDESTROYTRANSFERCONTEXT *psRGXDestroyTransferContextOUT,
					 CONNECTION_DATA *psConnection)
{












	psRGXDestroyTransferContextOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyTransferContextIN->hTransferContext,
					PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT);
	if ((psRGXDestroyTransferContextOUT->eError != PVRSRV_OK) && (psRGXDestroyTransferContextOUT->eError != PVRSRV_ERROR_RETRY))
	{
		PVR_ASSERT(0);
		goto RGXDestroyTransferContext_exit;
	}




RGXDestroyTransferContext_exit:



	return 0;
}


static IMG_INT
PVRSRVBridgeRGXSubmitTransfer(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXSUBMITTRANSFER *psRGXSubmitTransferIN,
					  PVRSRV_BRIDGE_OUT_RGXSUBMITTRANSFER *psRGXSubmitTransferOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hTransferContext = psRGXSubmitTransferIN->hTransferContext;
	RGX_SERVER_TQ_CONTEXT * psTransferContextInt = NULL;
	IMG_UINT32 *ui32ClientFenceCountInt = NULL;
	SYNC_PRIMITIVE_BLOCK * **psFenceUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE **hFenceUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 **ui32FenceSyncOffsetInt = NULL;
	IMG_UINT32 **ui32FenceValueInt = NULL;
	IMG_UINT32 *ui32ClientUpdateCountInt = NULL;
	SYNC_PRIMITIVE_BLOCK * **psUpdateUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE **hUpdateUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 **ui32UpdateSyncOffsetInt = NULL;
	IMG_UINT32 **ui32UpdateValueInt = NULL;
	IMG_UINT32 *ui32ServerSyncCountInt = NULL;
	IMG_UINT32 **ui32ServerSyncFlagsInt = NULL;
	SERVER_SYNC_PRIMITIVE * **psServerSyncInt = NULL;
	IMG_HANDLE **hServerSyncInt2 = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_UINT32 *ui32CommandSizeInt = NULL;
	IMG_UINT8 **ui8FWCommandInt = NULL;
	IMG_UINT32 *ui32TQPrepareFlagsInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR * *psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;




	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32ClientFenceCountInt = OSAllocZMemNoStats(psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32));
		if (!ui32ClientFenceCountInt)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXSubmitTransferIN->pui32ClientFenceCount, psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientFenceCountInt, psRGXSubmitTransferIN->pui32ClientFenceCount,
				psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;
		IMG_UINT32 ui32AllocSize2=0;
		IMG_UINT32 ui32Size2;
		IMG_UINT8 *pui8Ptr2 = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(SYNC_PRIMITIVE_BLOCK * *);
			ui32Size2 = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_HANDLE *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
				ui32AllocSize2 += ui32Size2;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				psFenceUFOSyncPrimBlockInt = (SYNC_PRIMITIVE_BLOCK * **) pui8Ptr;
				pui8Ptr += ui32Size;
				pui8Ptr2 = OSAllocMemNoStats(ui32AllocSize2);
				if (pui8Ptr2 == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				hFenceUFOSyncPrimBlockInt2 = (IMG_HANDLE **) pui8Ptr2;
				pui8Ptr2 += ui32Size2;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32ClientFenceCountInt[i] * sizeof(SYNC_PRIMITIVE_BLOCK *);		
				ui32Size2 = ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
						ui32AllocSize2 += ui32Size2;
					}
					else
					{
						psFenceUFOSyncPrimBlockInt[i] = (SYNC_PRIMITIVE_BLOCK * *) pui8Ptr;
						pui8Ptr += ui32Size;
						hFenceUFOSyncPrimBlockInt2[i] = (IMG_HANDLE *) pui8Ptr2;
						pui8Ptr2 += ui32Size2;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_HANDLE **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->phFenceUFOSyncPrimBlock[i], sizeof(IMG_HANDLE **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->phFenceUFOSyncPrimBlock[i],
				sizeof(IMG_HANDLE **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE)))
				|| (OSCopyFromUser(NULL, (hFenceUFOSyncPrimBlockInt2[i]), psPtr,
				(ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32 *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				ui32FenceSyncOffsetInt = (IMG_UINT32 **) pui8Ptr;
				pui8Ptr += ui32Size;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
					}
					else
					{
						ui32FenceSyncOffsetInt[i] = (IMG_UINT32 *) pui8Ptr;
						pui8Ptr += ui32Size;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->pui32FenceSyncOffset[i], sizeof(IMG_UINT32 **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->pui32FenceSyncOffset[i],
				sizeof(IMG_UINT32 **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32)))
				|| (OSCopyFromUser(NULL, (ui32FenceSyncOffsetInt[i]), psPtr,
				(ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32 *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				ui32FenceValueInt = (IMG_UINT32 **) pui8Ptr;
				pui8Ptr += ui32Size;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
					}
					else
					{
						ui32FenceValueInt[i] = (IMG_UINT32 *) pui8Ptr;
						pui8Ptr += ui32Size;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->pui32FenceValue[i], sizeof(IMG_UINT32 **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->pui32FenceValue[i],
				sizeof(IMG_UINT32 **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32)))
				|| (OSCopyFromUser(NULL, (ui32FenceValueInt[i]), psPtr,
				(ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32ClientUpdateCountInt = OSAllocZMemNoStats(psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32));
		if (!ui32ClientUpdateCountInt)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXSubmitTransferIN->pui32ClientUpdateCount, psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientUpdateCountInt, psRGXSubmitTransferIN->pui32ClientUpdateCount,
				psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;
		IMG_UINT32 ui32AllocSize2=0;
		IMG_UINT32 ui32Size2;
		IMG_UINT8 *pui8Ptr2 = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(SYNC_PRIMITIVE_BLOCK * *);
			ui32Size2 = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_HANDLE *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
				ui32AllocSize2 += ui32Size2;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				psUpdateUFOSyncPrimBlockInt = (SYNC_PRIMITIVE_BLOCK * **) pui8Ptr;
				pui8Ptr += ui32Size;
				pui8Ptr2 = OSAllocMemNoStats(ui32AllocSize2);
				if (pui8Ptr2 == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				hUpdateUFOSyncPrimBlockInt2 = (IMG_HANDLE **) pui8Ptr2;
				pui8Ptr2 += ui32Size2;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32ClientUpdateCountInt[i] * sizeof(SYNC_PRIMITIVE_BLOCK *);		
				ui32Size2 = ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
						ui32AllocSize2 += ui32Size2;
					}
					else
					{
						psUpdateUFOSyncPrimBlockInt[i] = (SYNC_PRIMITIVE_BLOCK * *) pui8Ptr;
						pui8Ptr += ui32Size;
						hUpdateUFOSyncPrimBlockInt2[i] = (IMG_HANDLE *) pui8Ptr2;
						pui8Ptr2 += ui32Size2;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_HANDLE **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->phUpdateUFOSyncPrimBlock[i], sizeof(IMG_HANDLE **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->phUpdateUFOSyncPrimBlock[i],
				sizeof(IMG_HANDLE **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE)))
				|| (OSCopyFromUser(NULL, (hUpdateUFOSyncPrimBlockInt2[i]), psPtr,
				(ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32 *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				ui32UpdateSyncOffsetInt = (IMG_UINT32 **) pui8Ptr;
				pui8Ptr += ui32Size;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
					}
					else
					{
						ui32UpdateSyncOffsetInt[i] = (IMG_UINT32 *) pui8Ptr;
						pui8Ptr += ui32Size;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->pui32UpdateSyncOffset[i], sizeof(IMG_UINT32 **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->pui32UpdateSyncOffset[i],
				sizeof(IMG_UINT32 **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32)))
				|| (OSCopyFromUser(NULL, (ui32UpdateSyncOffsetInt[i]), psPtr,
				(ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32 *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				ui32UpdateValueInt = (IMG_UINT32 **) pui8Ptr;
				pui8Ptr += ui32Size;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
					}
					else
					{
						ui32UpdateValueInt[i] = (IMG_UINT32 *) pui8Ptr;
						pui8Ptr += ui32Size;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->pui32UpdateValue[i], sizeof(IMG_UINT32 **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->pui32UpdateValue[i],
				sizeof(IMG_UINT32 **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32)))
				|| (OSCopyFromUser(NULL, (ui32UpdateValueInt[i]), psPtr,
				(ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32ServerSyncCountInt = OSAllocZMemNoStats(psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32));
		if (!ui32ServerSyncCountInt)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXSubmitTransferIN->pui32ServerSyncCount, psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ServerSyncCountInt, psRGXSubmitTransferIN->pui32ServerSyncCount,
				psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32 *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				ui32ServerSyncFlagsInt = (IMG_UINT32 **) pui8Ptr;
				pui8Ptr += ui32Size;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32ServerSyncCountInt[i] * sizeof(IMG_UINT32);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
					}
					else
					{
						ui32ServerSyncFlagsInt[i] = (IMG_UINT32 *) pui8Ptr;
						pui8Ptr += ui32Size;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->pui32ServerSyncFlags[i], sizeof(IMG_UINT32 **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->pui32ServerSyncFlags[i],
				sizeof(IMG_UINT32 **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32ServerSyncCountInt[i] * sizeof(IMG_UINT32)))
				|| (OSCopyFromUser(NULL, (ui32ServerSyncFlagsInt[i]), psPtr,
				(ui32ServerSyncCountInt[i] * sizeof(IMG_UINT32))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;
		IMG_UINT32 ui32AllocSize2=0;
		IMG_UINT32 ui32Size2;
		IMG_UINT8 *pui8Ptr2 = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(SERVER_SYNC_PRIMITIVE * *);
			ui32Size2 = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_HANDLE *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
				ui32AllocSize2 += ui32Size2;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				psServerSyncInt = (SERVER_SYNC_PRIMITIVE * **) pui8Ptr;
				pui8Ptr += ui32Size;
				pui8Ptr2 = OSAllocMemNoStats(ui32AllocSize2);
				if (pui8Ptr2 == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				hServerSyncInt2 = (IMG_HANDLE **) pui8Ptr2;
				pui8Ptr2 += ui32Size2;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32ServerSyncCountInt[i] * sizeof(SERVER_SYNC_PRIMITIVE *);		
				ui32Size2 = ui32ServerSyncCountInt[i] * sizeof(IMG_HANDLE);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
						ui32AllocSize2 += ui32Size2;
					}
					else
					{
						psServerSyncInt[i] = (SERVER_SYNC_PRIMITIVE * *) pui8Ptr;
						pui8Ptr += ui32Size;
						hServerSyncInt2[i] = (IMG_HANDLE *) pui8Ptr2;
						pui8Ptr2 += ui32Size2;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_HANDLE **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->phServerSync[i], sizeof(IMG_HANDLE **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->phServerSync[i],
				sizeof(IMG_HANDLE **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32ServerSyncCountInt[i] * sizeof(IMG_HANDLE)))
				|| (OSCopyFromUser(NULL, (hServerSyncInt2[i]), psPtr,
				(ui32ServerSyncCountInt[i] * sizeof(IMG_HANDLE))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	
	{
		uiUpdateFenceNameInt = OSAllocZMemNoStats(32 * sizeof(IMG_CHAR));
		if (!uiUpdateFenceNameInt)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXSubmitTransferIN->puiUpdateFenceName, 32 * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiUpdateFenceNameInt, psRGXSubmitTransferIN->puiUpdateFenceName,
				32 * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32CommandSizeInt = OSAllocZMemNoStats(psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32));
		if (!ui32CommandSizeInt)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXSubmitTransferIN->pui32CommandSize, psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32CommandSizeInt, psRGXSubmitTransferIN->pui32CommandSize,
				psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 ui32Pass=0;
		IMG_UINT32 i;
		IMG_UINT32 ui32AllocSize=0;
		IMG_UINT32 ui32Size;
		IMG_UINT8 *pui8Ptr = NULL;

		/*
			Two pass loop, 1st find out the size and 2nd allocation and set offsets.
			Keeps allocation cost down and simplifies the free path
		*/
		for (ui32Pass=0;ui32Pass<2;ui32Pass++)
		{
			ui32Size = psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT8 *);
			if (ui32Pass == 0)
			{
				ui32AllocSize += ui32Size;
			}
			else
			{
				pui8Ptr = OSAllocMemNoStats(ui32AllocSize);
				if (pui8Ptr == NULL)
				{
					psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
					goto RGXSubmitTransfer_exit;
				}
				ui8FWCommandInt = (IMG_UINT8 **) pui8Ptr;
				pui8Ptr += ui32Size;
			}
			
			for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
			{
				ui32Size = ui32CommandSizeInt[i] * sizeof(IMG_UINT8);		
				if (ui32Size)
				{
					if (ui32Pass == 0)
					{
						ui32AllocSize += ui32Size;
					}
					else
					{
						ui8FWCommandInt[i] = (IMG_UINT8 *) pui8Ptr;
						pui8Ptr += ui32Size;
					}
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT8 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			/* Copy the pointer over from the client side */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) &psRGXSubmitTransferIN->pui8FWCommand[i], sizeof(IMG_UINT8 **))
				|| (OSCopyFromUser(NULL, &psPtr, &psRGXSubmitTransferIN->pui8FWCommand[i],
				sizeof(IMG_UINT8 **)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psPtr, (ui32CommandSizeInt[i] * sizeof(IMG_UINT8)))
				|| (OSCopyFromUser(NULL, (ui8FWCommandInt[i]), psPtr,
				(ui32CommandSizeInt[i] * sizeof(IMG_UINT8))) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32TQPrepareFlagsInt = OSAllocZMemNoStats(psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32));
		if (!ui32TQPrepareFlagsInt)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXSubmitTransferIN->pui32TQPrepareFlags, psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32TQPrepareFlagsInt, psRGXSubmitTransferIN->pui32TQPrepareFlags,
				psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
	if (psRGXSubmitTransferIN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt = OSAllocZMemNoStats(psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32));
		if (!ui32SyncPMRFlagsInt)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXSubmitTransferIN->pui32SyncPMRFlags, psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32SyncPMRFlagsInt, psRGXSubmitTransferIN->pui32SyncPMRFlags,
				psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}
	if (psRGXSubmitTransferIN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt = OSAllocZMemNoStats(psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(PMR *));
		if (!psSyncPMRsInt)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
		hSyncPMRsInt2 = OSAllocMemNoStats(psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE));
		if (!hSyncPMRsInt2)
		{
			psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto RGXSubmitTransfer_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psRGXSubmitTransferIN->phSyncPMRs, psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hSyncPMRsInt2, psRGXSubmitTransferIN->phSyncPMRs,
				psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXSubmitTransferOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}






				{
					/* Look up the address from the handle */
					psRGXSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psTransferContextInt,
											hTransferContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT,
											IMG_TRUE);
					if(psRGXSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXSubmitTransfer_exit;
					}
				}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			IMG_UINT32 j;
			for (j=0;j<ui32ClientFenceCountInt[i];j++)
			{
				{
					/* Look up the address from the handle */
					psRGXSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psFenceUFOSyncPrimBlockInt[i][j],
											hFenceUFOSyncPrimBlockInt2[i][j],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXSubmitTransfer_exit;
					}
				}
			}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			IMG_UINT32 j;
			for (j=0;j<ui32ClientUpdateCountInt[i];j++)
			{
				{
					/* Look up the address from the handle */
					psRGXSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psUpdateUFOSyncPrimBlockInt[i][j],
											hUpdateUFOSyncPrimBlockInt2[i][j],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
											IMG_TRUE);
					if(psRGXSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXSubmitTransfer_exit;
					}
				}
			}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			IMG_UINT32 j;
			for (j=0;j<ui32ServerSyncCountInt[i];j++)
			{
				{
					/* Look up the address from the handle */
					psRGXSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psServerSyncInt[i][j],
											hServerSyncInt2[i][j],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
											IMG_TRUE);
					if(psRGXSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXSubmitTransfer_exit;
					}
				}
			}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXSubmitTransferIN->ui32SyncPMRCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXSubmitTransferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psSyncPMRsInt[i],
											hSyncPMRsInt2[i],
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psRGXSubmitTransferOUT->eError != PVRSRV_OK)
					{
						goto RGXSubmitTransfer_exit;
					}
				}
		}
	}

	psRGXSubmitTransferOUT->eError =
		PVRSRVRGXSubmitTransferKM(
					psTransferContextInt,
					psRGXSubmitTransferIN->ui32ClientCacheOpSeqNum,
					psRGXSubmitTransferIN->ui32PrepareCount,
					ui32ClientFenceCountInt,
					psFenceUFOSyncPrimBlockInt,
					ui32FenceSyncOffsetInt,
					ui32FenceValueInt,
					ui32ClientUpdateCountInt,
					psUpdateUFOSyncPrimBlockInt,
					ui32UpdateSyncOffsetInt,
					ui32UpdateValueInt,
					ui32ServerSyncCountInt,
					ui32ServerSyncFlagsInt,
					psServerSyncInt,
					psRGXSubmitTransferIN->i32CheckFenceFD,
					psRGXSubmitTransferIN->i32UpdateTimelineFD,
					&psRGXSubmitTransferOUT->i32UpdateFenceFD,
					uiUpdateFenceNameInt,
					ui32CommandSizeInt,
					ui8FWCommandInt,
					ui32TQPrepareFlagsInt,
					psRGXSubmitTransferIN->ui32ExtJobRef,
					psRGXSubmitTransferIN->ui32SyncPMRCount,
					ui32SyncPMRFlagsInt,
					psSyncPMRsInt);




RGXSubmitTransfer_exit:






				{
					/* Unreference the previously looked up handle */
						if(psTransferContextInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hTransferContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT);
						}
				}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			IMG_UINT32 j;
			for (j=0;j<ui32ClientFenceCountInt[i];j++)
			{
				{
					/* Unreference the previously looked up handle */
						if(psFenceUFOSyncPrimBlockInt[i][j])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hFenceUFOSyncPrimBlockInt2[i][j],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
			}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			IMG_UINT32 j;
			for (j=0;j<ui32ClientUpdateCountInt[i];j++)
			{
				{
					/* Unreference the previously looked up handle */
						if(psUpdateUFOSyncPrimBlockInt[i][j])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hUpdateUFOSyncPrimBlockInt2[i][j],
											PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
						}
				}
			}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXSubmitTransferIN->ui32PrepareCount;i++)
		{
			IMG_UINT32 j;
			for (j=0;j<ui32ServerSyncCountInt[i];j++)
			{
				{
					/* Unreference the previously looked up handle */
						if(psServerSyncInt[i][j])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hServerSyncInt2[i][j],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
						}
				}
			}
		}
	}





	{
		IMG_UINT32 i;

		for (i=0;i<psRGXSubmitTransferIN->ui32SyncPMRCount;i++)
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

	if (ui32ClientFenceCountInt)
		OSFreeMemNoStats(ui32ClientFenceCountInt);
	if (psFenceUFOSyncPrimBlockInt)
		OSFreeMemNoStats(psFenceUFOSyncPrimBlockInt);
	if (hFenceUFOSyncPrimBlockInt2)
		OSFreeMemNoStats(hFenceUFOSyncPrimBlockInt2);
	if (ui32FenceSyncOffsetInt)
		OSFreeMemNoStats(ui32FenceSyncOffsetInt);
	if (ui32FenceValueInt)
		OSFreeMemNoStats(ui32FenceValueInt);
	if (ui32ClientUpdateCountInt)
		OSFreeMemNoStats(ui32ClientUpdateCountInt);
	if (psUpdateUFOSyncPrimBlockInt)
		OSFreeMemNoStats(psUpdateUFOSyncPrimBlockInt);
	if (hUpdateUFOSyncPrimBlockInt2)
		OSFreeMemNoStats(hUpdateUFOSyncPrimBlockInt2);
	if (ui32UpdateSyncOffsetInt)
		OSFreeMemNoStats(ui32UpdateSyncOffsetInt);
	if (ui32UpdateValueInt)
		OSFreeMemNoStats(ui32UpdateValueInt);
	if (ui32ServerSyncCountInt)
		OSFreeMemNoStats(ui32ServerSyncCountInt);
	if (ui32ServerSyncFlagsInt)
		OSFreeMemNoStats(ui32ServerSyncFlagsInt);
	if (psServerSyncInt)
		OSFreeMemNoStats(psServerSyncInt);
	if (hServerSyncInt2)
		OSFreeMemNoStats(hServerSyncInt2);
	if (uiUpdateFenceNameInt)
		OSFreeMemNoStats(uiUpdateFenceNameInt);
	if (ui32CommandSizeInt)
		OSFreeMemNoStats(ui32CommandSizeInt);
	if (ui8FWCommandInt)
		OSFreeMemNoStats(ui8FWCommandInt);
	if (ui32TQPrepareFlagsInt)
		OSFreeMemNoStats(ui32TQPrepareFlagsInt);
	if (ui32SyncPMRFlagsInt)
		OSFreeMemNoStats(ui32SyncPMRFlagsInt);
	if (psSyncPMRsInt)
		OSFreeMemNoStats(psSyncPMRsInt);
	if (hSyncPMRsInt2)
		OSFreeMemNoStats(hSyncPMRsInt2);

	return 0;
}


static IMG_INT
PVRSRVBridgeRGXSetTransferContextPriority(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXSETTRANSFERCONTEXTPRIORITY *psRGXSetTransferContextPriorityIN,
					  PVRSRV_BRIDGE_OUT_RGXSETTRANSFERCONTEXTPRIORITY *psRGXSetTransferContextPriorityOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hTransferContext = psRGXSetTransferContextPriorityIN->hTransferContext;
	RGX_SERVER_TQ_CONTEXT * psTransferContextInt = NULL;










				{
					/* Look up the address from the handle */
					psRGXSetTransferContextPriorityOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psTransferContextInt,
											hTransferContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT,
											IMG_TRUE);
					if(psRGXSetTransferContextPriorityOUT->eError != PVRSRV_OK)
					{
						goto RGXSetTransferContextPriority_exit;
					}
				}

	psRGXSetTransferContextPriorityOUT->eError =
		PVRSRVRGXSetTransferContextPriorityKM(psConnection, OSGetDevData(psConnection),
					psTransferContextInt,
					psRGXSetTransferContextPriorityIN->ui32Priority);




RGXSetTransferContextPriority_exit:






				{
					/* Unreference the previously looked up handle */
						if(psTransferContextInt)
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hTransferContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT);
						}
				}


	return 0;
}




/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */

static IMG_BOOL bUseLock = IMG_TRUE;

PVRSRV_ERROR InitRGXTQBridge(void);
PVRSRV_ERROR DeinitRGXTQBridge(void);

/*
 * Register all RGXTQ functions with services
 */
PVRSRV_ERROR InitRGXTQBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ, PVRSRV_BRIDGE_RGXTQ_RGXCREATETRANSFERCONTEXT, PVRSRVBridgeRGXCreateTransferContext,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ, PVRSRV_BRIDGE_RGXTQ_RGXDESTROYTRANSFERCONTEXT, PVRSRVBridgeRGXDestroyTransferContext,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ, PVRSRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER, PVRSRVBridgeRGXSubmitTransfer,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ, PVRSRV_BRIDGE_RGXTQ_RGXSETTRANSFERCONTEXTPRIORITY, PVRSRVBridgeRGXSetTransferContextPriority,
					NULL, bUseLock);


	return PVRSRV_OK;
}

/*
 * Unregister all rgxtq functions with services
 */
PVRSRV_ERROR DeinitRGXTQBridge(void)
{
	return PVRSRV_OK;
}
