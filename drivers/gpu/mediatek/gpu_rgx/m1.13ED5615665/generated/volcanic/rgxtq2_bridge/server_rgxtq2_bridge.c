/*******************************************************************************
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
*******************************************************************************/

#include <linux/uaccess.h>

#include "img_defs.h"

#include "rgxtdmtransfer.h"

#include "common_rgxtq2_bridge.h"

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
PVRSRVBridgeRGXTDMCreateTransferContext(IMG_UINT32 ui32DispatchTableEntry,
					PVRSRV_BRIDGE_IN_RGXTDMCREATETRANSFERCONTEXT
					* psRGXTDMCreateTransferContextIN,
					PVRSRV_BRIDGE_OUT_RGXTDMCREATETRANSFERCONTEXT
					* psRGXTDMCreateTransferContextOUT,
					CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hPrivData = psRGXTDMCreateTransferContextIN->hPrivData;
	IMG_HANDLE hPrivDataInt = NULL;
	RGX_SERVER_TQ_TDM_CONTEXT *psTransferContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXTDMCreateTransferContextOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&hPrivDataInt,
				       hPrivData,
				       PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
				       IMG_TRUE);
	if (unlikely(psRGXTDMCreateTransferContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMCreateTransferContext_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXTDMCreateTransferContextOUT->eError =
	    PVRSRVRGXTDMCreateTransferContextKM(psConnection,
						OSGetDevNode(psConnection),
						psRGXTDMCreateTransferContextIN->
						ui32Priority, hPrivDataInt,
						psRGXTDMCreateTransferContextIN->
						ui32PackedCCBSizeU88,
						psRGXTDMCreateTransferContextIN->
						ui32ContextFlags,
						&psTransferContextInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXTDMCreateTransferContextOUT->eError != PVRSRV_OK))
	{
		goto RGXTDMCreateTransferContext_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXTDMCreateTransferContextOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXTDMCreateTransferContextOUT->
				      hTransferContext,
				      (void *)psTransferContextInt,
				      PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      PVRSRVRGXTDMDestroyTransferContextKM);
	if (unlikely(psRGXTDMCreateTransferContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMCreateTransferContext_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXTDMCreateTransferContext_exit:

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

	if (psRGXTDMCreateTransferContextOUT->eError != PVRSRV_OK)
	{
		if (psTransferContextInt)
		{
			PVRSRVRGXTDMDestroyTransferContextKM
			    (psTransferContextInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXTDMDestroyTransferContext(IMG_UINT32 ui32DispatchTableEntry,
					 PVRSRV_BRIDGE_IN_RGXTDMDESTROYTRANSFERCONTEXT
					 * psRGXTDMDestroyTransferContextIN,
					 PVRSRV_BRIDGE_OUT_RGXTDMDESTROYTRANSFERCONTEXT
					 * psRGXTDMDestroyTransferContextOUT,
					 CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXTDMDestroyTransferContextOUT->eError =
	    PVRSRVReleaseHandleStagedUnlock(psConnection->psHandleBase,
					    (IMG_HANDLE)
					    psRGXTDMDestroyTransferContextIN->
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
	if (unlikely
	    ((psRGXTDMDestroyTransferContextOUT->eError != PVRSRV_OK)
	     && (psRGXTDMDestroyTransferContextOUT->eError !=
		 PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeRGXTDMDestroyTransferContext: %s",
			 PVRSRVGetErrorString
			 (psRGXTDMDestroyTransferContextOUT->eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMDestroyTransferContext_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXTDMDestroyTransferContext_exit:

	return 0;
}

#if defined(SUPPORT_SERVER_SYNC_IMPL)
static IMG_INT
PVRSRVBridgeRGXTDMSubmitTransfer(IMG_UINT32 ui32DispatchTableEntry,
				 PVRSRV_BRIDGE_IN_RGXTDMSUBMITTRANSFER *
				 psRGXTDMSubmitTransferIN,
				 PVRSRV_BRIDGE_OUT_RGXTDMSUBMITTRANSFER *
				 psRGXTDMSubmitTransferOUT,
				 CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hTransferContext =
	    psRGXTDMSubmitTransferIN->hTransferContext;
	RGX_SERVER_TQ_TDM_CONTEXT *psTransferContextInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psFenceUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE *hFenceUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32FenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32FenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psUpdateUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE *hUpdateUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32UpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32UpdateValueInt = NULL;
	IMG_UINT32 *ui32ServerSyncFlagsInt = NULL;
	SERVER_SYNC_PRIMITIVE **psServerSyncInt = NULL;
	IMG_HANDLE *hServerSyncInt2 = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_UINT8 *ui8FWCommandInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR **psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
	     sizeof(IMG_HANDLE)) +
	    (psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
	     sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
	     sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
	     sizeof(IMG_HANDLE)) +
	    (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
	     sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
	     sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransferIN->ui32ServerSyncCount *
	     sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransferIN->ui32ServerSyncCount *
	     sizeof(SERVER_SYNC_PRIMITIVE *)) +
	    (psRGXTDMSubmitTransferIN->ui32ServerSyncCount *
	     sizeof(IMG_HANDLE)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (psRGXTDMSubmitTransferIN->ui32CommandSize * sizeof(IMG_UINT8)) +
	    (psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(PMR *)) +
	    (psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) +
	    0;

	if (unlikely
	    (psRGXTDMSubmitTransferIN->ui32ClientFenceCount >
	     PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXTDMSubmitTransferOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer_exit;
	}

	if (unlikely
	    (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount >
	     PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXTDMSubmitTransferOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer_exit;
	}

	if (unlikely
	    (psRGXTDMSubmitTransferIN->ui32ServerSyncCount >
	     PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXTDMSubmitTransferOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer_exit;
	}

	if (unlikely
	    (psRGXTDMSubmitTransferIN->ui32CommandSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXTDMSubmitTransferOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer_exit;
	}

	if (unlikely
	    (psRGXTDMSubmitTransferIN->ui32SyncPMRCount >
	     PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXTDMSubmitTransferOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXTDMSubmitTransferIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psRGXTDMSubmitTransferIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXTDMSubmitTransferOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXTDMSubmitTransfer_exit;
			}
		}
	}

	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount != 0)
	{
		psFenceUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hFenceUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
	    sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hFenceUFOSyncPrimBlockInt2,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     phFenceUFOSyncPrimBlock,
		     psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount != 0)
	{
		ui32FenceSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32FenceSyncOffsetInt,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     pui32FenceSyncOffset,
		     psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount != 0)
	{
		ui32FenceValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32FenceValueInt,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     pui32FenceValue,
		     psRGXTDMSubmitTransferIN->ui32ClientFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount != 0)
	{
		psUpdateUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hUpdateUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
	    sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hUpdateUFOSyncPrimBlockInt2,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     phUpdateUFOSyncPrimBlock,
		     psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount != 0)
	{
		ui32UpdateSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32UpdateSyncOffsetInt,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     pui32UpdateSyncOffset,
		     psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount != 0)
	{
		ui32UpdateValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32UpdateValueInt,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     pui32UpdateValue,
		     psRGXTDMSubmitTransferIN->ui32ClientUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32ServerSyncCount != 0)
	{
		ui32ServerSyncFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ServerSyncCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(IMG_UINT32) >
	    0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ServerSyncFlagsInt,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     pui32ServerSyncFlags,
		     psRGXTDMSubmitTransferIN->ui32ServerSyncCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32ServerSyncCount != 0)
	{
		psServerSyncInt =
		    (SERVER_SYNC_PRIMITIVE **) (((IMG_UINT8 *) pArrayArgsBuffer)
						+ ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ServerSyncCount *
		    sizeof(SERVER_SYNC_PRIMITIVE *);
		hServerSyncInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32ServerSyncCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32ServerSyncCount * sizeof(IMG_HANDLE) >
	    0)
	{
		if (OSCopyFromUser
		    (NULL, hServerSyncInt2,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     phServerSync,
		     psRGXTDMSubmitTransferIN->ui32ServerSyncCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
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
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     puiUpdateFenceName,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceNameInt)[(PVRSRV_SYNC_NAME_LENGTH *
					sizeof(IMG_CHAR)) - 1] = '\0';
	}
	if (psRGXTDMSubmitTransferIN->ui32CommandSize != 0)
	{
		ui8FWCommandInt =
		    (IMG_UINT8 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				   ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32CommandSize *
		    sizeof(IMG_UINT8);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32CommandSize * sizeof(IMG_UINT8) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui8FWCommandInt,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     pui8FWCommand,
		     psRGXTDMSubmitTransferIN->ui32CommandSize *
		     sizeof(IMG_UINT8)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32SyncPMRCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32SyncPMRFlagsInt,
		     (const void __user *)psRGXTDMSubmitTransferIN->
		     pui32SyncPMRFlags,
		     psRGXTDMSubmitTransferIN->ui32SyncPMRCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}
	if (psRGXTDMSubmitTransferIN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt =
		    (PMR **) (((IMG_UINT8 *) pArrayArgsBuffer) +
			      ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(PMR *);
		hSyncPMRsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransferIN->ui32SyncPMRCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hSyncPMRsInt2,
		     (const void __user *)psRGXTDMSubmitTransferIN->phSyncPMRs,
		     psRGXTDMSubmitTransferIN->ui32SyncPMRCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXTDMSubmitTransferOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psTransferContextInt,
				       hTransferContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMSubmitTransfer_exit;
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransferIN->ui32ClientFenceCount;
		     i++)
		{
			/* Look up the address from the handle */
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psFenceUFOSyncPrimBlockInt
						       [i],
						       hFenceUFOSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely
			    (psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXTDMSubmitTransfer_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransferIN->ui32ClientUpdateCount;
		     i++)
		{
			/* Look up the address from the handle */
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psUpdateUFOSyncPrimBlockInt
						       [i],
						       hUpdateUFOSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely
			    (psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXTDMSubmitTransfer_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransferIN->ui32ServerSyncCount;
		     i++)
		{
			/* Look up the address from the handle */
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psServerSyncInt[i],
						       hServerSyncInt2[i],
						       PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
						       IMG_TRUE);
			if (unlikely
			    (psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXTDMSubmitTransfer_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransferIN->ui32SyncPMRCount; i++)
		{
			/* Look up the address from the handle */
			psRGXTDMSubmitTransferOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psSyncPMRsInt[i],
						       hSyncPMRsInt2[i],
						       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
						       IMG_TRUE);
			if (unlikely
			    (psRGXTDMSubmitTransferOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXTDMSubmitTransfer_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXTDMSubmitTransferOUT->eError =
	    PVRSRVRGXTDMSubmitTransferKM(psTransferContextInt,
					 psRGXTDMSubmitTransferIN->
					 ui32PDumpFlags,
					 psRGXTDMSubmitTransferIN->
					 ui32ClientCacheOpSeqNum,
					 psRGXTDMSubmitTransferIN->
					 ui32ClientFenceCount,
					 psFenceUFOSyncPrimBlockInt,
					 ui32FenceSyncOffsetInt,
					 ui32FenceValueInt,
					 psRGXTDMSubmitTransferIN->
					 ui32ClientUpdateCount,
					 psUpdateUFOSyncPrimBlockInt,
					 ui32UpdateSyncOffsetInt,
					 ui32UpdateValueInt,
					 psRGXTDMSubmitTransferIN->
					 ui32ServerSyncCount,
					 ui32ServerSyncFlagsInt,
					 psServerSyncInt,
					 psRGXTDMSubmitTransferIN->
					 hCheckFenceFD,
					 psRGXTDMSubmitTransferIN->
					 hUpdateTimeline,
					 &psRGXTDMSubmitTransferOUT->
					 hUpdateFence, uiUpdateFenceNameInt,
					 psRGXTDMSubmitTransferIN->
					 ui32CommandSize, ui8FWCommandInt,
					 psRGXTDMSubmitTransferIN->
					 ui32ExternalJobReference,
					 psRGXTDMSubmitTransferIN->
					 ui32SyncPMRCount, ui32SyncPMRFlagsInt,
					 psSyncPMRsInt,
					 psRGXTDMSubmitTransferIN->
					 ui32Characteristic1,
					 psRGXTDMSubmitTransferIN->
					 ui32Characteristic2,
					 psRGXTDMSubmitTransferIN->
					 ui64DeadlineInus);

RGXTDMSubmitTransfer_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psTransferContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
	}

	if (hFenceUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransferIN->ui32ClientFenceCount;
		     i++)
		{

			/* Unreference the previously looked up handle */
			if (hFenceUFOSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hFenceUFOSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hUpdateUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransferIN->ui32ClientUpdateCount;
		     i++)
		{

			/* Unreference the previously looked up handle */
			if (hUpdateUFOSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hUpdateUFOSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hServerSyncInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransferIN->ui32ServerSyncCount;
		     i++)
		{

			/* Unreference the previously looked up handle */
			if (hServerSyncInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hServerSyncInt2[i],
							    PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
			}
		}
	}

	if (hSyncPMRsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransferIN->ui32SyncPMRCount; i++)
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
#define PVRSRVBridgeRGXTDMSubmitTransfer NULL
#endif

static IMG_INT
PVRSRVBridgeRGXTDMSetTransferContextPriority(IMG_UINT32 ui32DispatchTableEntry,
					     PVRSRV_BRIDGE_IN_RGXTDMSETTRANSFERCONTEXTPRIORITY
					     *
					     psRGXTDMSetTransferContextPriorityIN,
					     PVRSRV_BRIDGE_OUT_RGXTDMSETTRANSFERCONTEXTPRIORITY
					     *
					     psRGXTDMSetTransferContextPriorityOUT,
					     CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hTransferContext =
	    psRGXTDMSetTransferContextPriorityIN->hTransferContext;
	RGX_SERVER_TQ_TDM_CONTEXT *psTransferContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXTDMSetTransferContextPriorityOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psTransferContextInt,
				       hTransferContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
				       IMG_TRUE);
	if (unlikely
	    (psRGXTDMSetTransferContextPriorityOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMSetTransferContextPriority_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXTDMSetTransferContextPriorityOUT->eError =
	    PVRSRVRGXTDMSetTransferContextPriorityKM(psConnection,
						     OSGetDevNode(psConnection),
						     psTransferContextInt,
						     psRGXTDMSetTransferContextPriorityIN->
						     ui32Priority);

RGXTDMSetTransferContextPriority_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psTransferContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXTDMNotifyWriteOffsetUpdate(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXTDMNOTIFYWRITEOFFSETUPDATE
					  * psRGXTDMNotifyWriteOffsetUpdateIN,
					  PVRSRV_BRIDGE_OUT_RGXTDMNOTIFYWRITEOFFSETUPDATE
					  * psRGXTDMNotifyWriteOffsetUpdateOUT,
					  CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hTransferContext =
	    psRGXTDMNotifyWriteOffsetUpdateIN->hTransferContext;
	RGX_SERVER_TQ_TDM_CONTEXT *psTransferContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXTDMNotifyWriteOffsetUpdateOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psTransferContextInt,
				       hTransferContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXTDMNotifyWriteOffsetUpdateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMNotifyWriteOffsetUpdate_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXTDMNotifyWriteOffsetUpdateOUT->eError =
	    PVRSRVRGXTDMNotifyWriteOffsetUpdateKM(psTransferContextInt,
						  psRGXTDMNotifyWriteOffsetUpdateIN->
						  ui32PDumpFlags);

RGXTDMNotifyWriteOffsetUpdate_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psTransferContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

#if !defined(SUPPORT_SERVER_SYNC_IMPL)
static IMG_INT
PVRSRVBridgeRGXTDMSubmitTransfer2(IMG_UINT32 ui32DispatchTableEntry,
				  PVRSRV_BRIDGE_IN_RGXTDMSUBMITTRANSFER2 *
				  psRGXTDMSubmitTransfer2IN,
				  PVRSRV_BRIDGE_OUT_RGXTDMSUBMITTRANSFER2 *
				  psRGXTDMSubmitTransfer2OUT,
				  CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hTransferContext =
	    psRGXTDMSubmitTransfer2IN->hTransferContext;
	RGX_SERVER_TQ_TDM_CONTEXT *psTransferContextInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psFenceUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE *hFenceUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32FenceSyncOffsetInt = NULL;
	IMG_UINT32 *ui32FenceValueInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psUpdateUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE *hUpdateUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32UpdateSyncOffsetInt = NULL;
	IMG_UINT32 *ui32UpdateValueInt = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_UINT8 *ui8FWCommandInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR **psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
	     sizeof(IMG_HANDLE)) +
	    (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
	     sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
	     sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
	     sizeof(IMG_HANDLE)) +
	    (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
	     sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
	     sizeof(IMG_UINT32)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (psRGXTDMSubmitTransfer2IN->ui32CommandSize * sizeof(IMG_UINT8)) +
	    (psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount * sizeof(IMG_UINT32)) +
	    (psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount * sizeof(PMR *)) +
	    (psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) +
	    0;

	if (unlikely
	    (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount >
	     PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXTDMSubmitTransfer2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer2_exit;
	}

	if (unlikely
	    (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount >
	     PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXTDMSubmitTransfer2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer2_exit;
	}

	if (unlikely
	    (psRGXTDMSubmitTransfer2IN->ui32CommandSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXTDMSubmitTransfer2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer2_exit;
	}

	if (unlikely
	    (psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount >
	     PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXTDMSubmitTransfer2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXTDMSubmitTransfer2_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXTDMSubmitTransfer2IN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psRGXTDMSubmitTransfer2IN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXTDMSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXTDMSubmitTransfer2_exit;
			}
		}
	}

	if (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount != 0)
	{
		psFenceUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hFenceUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
	    sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hFenceUFOSyncPrimBlockInt2,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     phFenceUFOSyncPrimBlock,
		     psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
	}
	if (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount != 0)
	{
		ui32FenceSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32FenceSyncOffsetInt,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     pui32FenceSyncOffset,
		     psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
	}
	if (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount != 0)
	{
		ui32FenceValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32FenceValueInt,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     pui32FenceValue,
		     psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
	}
	if (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount != 0)
	{
		psUpdateUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) (((IMG_UINT8 *) pArrayArgsBuffer)
					       + ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hUpdateUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
	    sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hUpdateUFOSyncPrimBlockInt2,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     phUpdateUFOSyncPrimBlock,
		     psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
	}
	if (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount != 0)
	{
		ui32UpdateSyncOffsetInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32UpdateSyncOffsetInt,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     pui32UpdateSyncOffset,
		     psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
	}
	if (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount != 0)
	{
		ui32UpdateValueInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
	    sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32UpdateValueInt,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     pui32UpdateValue,
		     psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
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
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     puiUpdateFenceName,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceNameInt)[(PVRSRV_SYNC_NAME_LENGTH *
					sizeof(IMG_CHAR)) - 1] = '\0';
	}
	if (psRGXTDMSubmitTransfer2IN->ui32CommandSize != 0)
	{
		ui8FWCommandInt =
		    (IMG_UINT8 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				   ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32CommandSize *
		    sizeof(IMG_UINT8);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32CommandSize * sizeof(IMG_UINT8) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui8FWCommandInt,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     pui8FWCommand,
		     psRGXTDMSubmitTransfer2IN->ui32CommandSize *
		     sizeof(IMG_UINT8)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
	}
	if (psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount * sizeof(IMG_UINT32) >
	    0)
	{
		if (OSCopyFromUser
		    (NULL, ui32SyncPMRFlagsInt,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->
		     pui32SyncPMRFlags,
		     psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
	}
	if (psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt =
		    (PMR **) (((IMG_UINT8 *) pArrayArgsBuffer) +
			      ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount * sizeof(PMR *);
		hSyncPMRsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount * sizeof(IMG_HANDLE) >
	    0)
	{
		if (OSCopyFromUser
		    (NULL, hSyncPMRsInt2,
		     (const void __user *)psRGXTDMSubmitTransfer2IN->phSyncPMRs,
		     psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXTDMSubmitTransfer2_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXTDMSubmitTransfer2OUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psTransferContextInt,
				       hTransferContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXTDMSubmitTransfer2OUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMSubmitTransfer2_exit;
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount;
		     i++)
		{
			/* Look up the address from the handle */
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psFenceUFOSyncPrimBlockInt
						       [i],
						       hFenceUFOSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely
			    (psRGXTDMSubmitTransfer2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXTDMSubmitTransfer2_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0;
		     i < psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount; i++)
		{
			/* Look up the address from the handle */
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psUpdateUFOSyncPrimBlockInt
						       [i],
						       hUpdateUFOSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely
			    (psRGXTDMSubmitTransfer2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXTDMSubmitTransfer2_exit;
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount;
		     i++)
		{
			/* Look up the address from the handle */
			psRGXTDMSubmitTransfer2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psSyncPMRsInt[i],
						       hSyncPMRsInt2[i],
						       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
						       IMG_TRUE);
			if (unlikely
			    (psRGXTDMSubmitTransfer2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXTDMSubmitTransfer2_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXTDMSubmitTransfer2OUT->eError =
	    PVRSRVRGXTDMSubmitTransferKM(psTransferContextInt,
					 psRGXTDMSubmitTransfer2IN->
					 ui32PDumpFlags,
					 psRGXTDMSubmitTransfer2IN->
					 ui32ClientCacheOpSeqNum,
					 psRGXTDMSubmitTransfer2IN->
					 ui32ClientFenceCount,
					 psFenceUFOSyncPrimBlockInt,
					 ui32FenceSyncOffsetInt,
					 ui32FenceValueInt,
					 psRGXTDMSubmitTransfer2IN->
					 ui32ClientUpdateCount,
					 psUpdateUFOSyncPrimBlockInt,
					 ui32UpdateSyncOffsetInt,
					 ui32UpdateValueInt,
					 psRGXTDMSubmitTransfer2IN->
					 hCheckFenceFD,
					 psRGXTDMSubmitTransfer2IN->
					 hUpdateTimeline,
					 &psRGXTDMSubmitTransfer2OUT->
					 hUpdateFence, uiUpdateFenceNameInt,
					 psRGXTDMSubmitTransfer2IN->
					 ui32CommandSize, ui8FWCommandInt,
					 psRGXTDMSubmitTransfer2IN->
					 ui32ExternalJobReference,
					 psRGXTDMSubmitTransfer2IN->
					 ui32SyncPMRCount, ui32SyncPMRFlagsInt,
					 psSyncPMRsInt,
					 psRGXTDMSubmitTransfer2IN->
					 ui32Characteristic1,
					 psRGXTDMSubmitTransfer2IN->
					 ui32Characteristic2,
					 psRGXTDMSubmitTransfer2IN->
					 ui64DeadlineInus);

RGXTDMSubmitTransfer2_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psTransferContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
	}

	if (hFenceUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransfer2IN->ui32ClientFenceCount;
		     i++)
		{

			/* Unreference the previously looked up handle */
			if (hFenceUFOSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hFenceUFOSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hUpdateUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0;
		     i < psRGXTDMSubmitTransfer2IN->ui32ClientUpdateCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hUpdateUFOSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hUpdateUFOSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}

	if (hSyncPMRsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXTDMSubmitTransfer2IN->ui32SyncPMRCount;
		     i++)
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
#define PVRSRVBridgeRGXTDMSubmitTransfer2 NULL
#endif

static IMG_INT
PVRSRVBridgeRGXTDMGetSharedMemory(IMG_UINT32 ui32DispatchTableEntry,
				  PVRSRV_BRIDGE_IN_RGXTDMGETSHAREDMEMORY *
				  psRGXTDMGetSharedMemoryIN,
				  PVRSRV_BRIDGE_OUT_RGXTDMGETSHAREDMEMORY *
				  psRGXTDMGetSharedMemoryOUT,
				  CONNECTION_DATA * psConnection)
{
	PMR *psCLIPMRMemInt = NULL;
	PMR *psUSCPMRMemInt = NULL;

	PVR_UNREFERENCED_PARAMETER(psRGXTDMGetSharedMemoryIN);

	psRGXTDMGetSharedMemoryOUT->eError =
	    PVRSRVRGXTDMGetSharedMemoryKM(psConnection,
					  OSGetDevNode(psConnection),
					  &psCLIPMRMemInt, &psUSCPMRMemInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXTDMGetSharedMemoryOUT->eError != PVRSRV_OK))
	{
		goto RGXTDMGetSharedMemory_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXTDMGetSharedMemoryOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXTDMGetSharedMemoryOUT->hCLIPMRMem,
				      (void *)psCLIPMRMemInt,
				      PVRSRV_HANDLE_TYPE_PMR_LOCAL_EXPORT_HANDLE,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      PVRSRVRGXTDMReleaseSharedMemoryKM);
	if (unlikely(psRGXTDMGetSharedMemoryOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMGetSharedMemory_exit;
	}

	psRGXTDMGetSharedMemoryOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXTDMGetSharedMemoryOUT->hUSCPMRMem,
				      (void *)psUSCPMRMemInt,
				      PVRSRV_HANDLE_TYPE_PMR_LOCAL_EXPORT_HANDLE,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      PVRSRVRGXTDMReleaseSharedMemoryKM);
	if (unlikely(psRGXTDMGetSharedMemoryOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMGetSharedMemory_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXTDMGetSharedMemory_exit:

	if (psRGXTDMGetSharedMemoryOUT->eError != PVRSRV_OK)
	{
		if (psCLIPMRMemInt)
		{
			PVRSRVRGXTDMReleaseSharedMemoryKM(psCLIPMRMemInt);
		}
		if (psUSCPMRMemInt)
		{
			PVRSRVRGXTDMReleaseSharedMemoryKM(psUSCPMRMemInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXTDMReleaseSharedMemory(IMG_UINT32 ui32DispatchTableEntry,
				      PVRSRV_BRIDGE_IN_RGXTDMRELEASESHAREDMEMORY
				      * psRGXTDMReleaseSharedMemoryIN,
				      PVRSRV_BRIDGE_OUT_RGXTDMRELEASESHAREDMEMORY
				      * psRGXTDMReleaseSharedMemoryOUT,
				      CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXTDMReleaseSharedMemoryOUT->eError =
	    PVRSRVReleaseHandleStagedUnlock(psConnection->psHandleBase,
					    (IMG_HANDLE)
					    psRGXTDMReleaseSharedMemoryIN->
					    hPMRMem,
					    PVRSRV_HANDLE_TYPE_PMR_LOCAL_EXPORT_HANDLE);
	if (unlikely
	    ((psRGXTDMReleaseSharedMemoryOUT->eError != PVRSRV_OK)
	     && (psRGXTDMReleaseSharedMemoryOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeRGXTDMReleaseSharedMemory: %s",
			 PVRSRVGetErrorString(psRGXTDMReleaseSharedMemoryOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMReleaseSharedMemory_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXTDMReleaseSharedMemory_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXTDMSetTransferContextProperty(IMG_UINT32 ui32DispatchTableEntry,
					     PVRSRV_BRIDGE_IN_RGXTDMSETTRANSFERCONTEXTPROPERTY
					     *
					     psRGXTDMSetTransferContextPropertyIN,
					     PVRSRV_BRIDGE_OUT_RGXTDMSETTRANSFERCONTEXTPROPERTY
					     *
					     psRGXTDMSetTransferContextPropertyOUT,
					     CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hTransferContext =
	    psRGXTDMSetTransferContextPropertyIN->hTransferContext;
	RGX_SERVER_TQ_TDM_CONTEXT *psTransferContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXTDMSetTransferContextPropertyOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psTransferContextInt,
				       hTransferContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT,
				       IMG_TRUE);
	if (unlikely
	    (psRGXTDMSetTransferContextPropertyOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXTDMSetTransferContextProperty_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXTDMSetTransferContextPropertyOUT->eError =
	    PVRSRVRGXTDMSetTransferContextPropertyKM(psTransferContextInt,
						     psRGXTDMSetTransferContextPropertyIN->
						     ui32Property,
						     psRGXTDMSetTransferContextPropertyIN->
						     ui64Input,
						     &psRGXTDMSetTransferContextPropertyOUT->
						     ui64Output);

RGXTDMSetTransferContextProperty_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psTransferContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_TDM_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR InitRGXTQ2Bridge(void);
PVRSRV_ERROR DeinitRGXTQ2Bridge(void);

/*
 * Register all RGXTQ2 functions with services
 */
PVRSRV_ERROR InitRGXTQ2Bridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMCREATETRANSFERCONTEXT,
			      PVRSRVBridgeRGXTDMCreateTransferContext, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMDESTROYTRANSFERCONTEXT,
			      PVRSRVBridgeRGXTDMDestroyTransferContext, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMSUBMITTRANSFER,
			      PVRSRVBridgeRGXTDMSubmitTransfer, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMSETTRANSFERCONTEXTPRIORITY,
			      PVRSRVBridgeRGXTDMSetTransferContextPriority,
			      NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMNOTIFYWRITEOFFSETUPDATE,
			      PVRSRVBridgeRGXTDMNotifyWriteOffsetUpdate, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMSUBMITTRANSFER2,
			      PVRSRVBridgeRGXTDMSubmitTransfer2, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMGETSHAREDMEMORY,
			      PVRSRVBridgeRGXTDMGetSharedMemory, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMRELEASESHAREDMEMORY,
			      PVRSRVBridgeRGXTDMReleaseSharedMemory, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
			      PVRSRV_BRIDGE_RGXTQ2_RGXTDMSETTRANSFERCONTEXTPROPERTY,
			      PVRSRVBridgeRGXTDMSetTransferContextProperty,
			      NULL);

	return PVRSRV_OK;
}

/*
 * Unregister all rgxtq2 functions with services
 */
PVRSRV_ERROR DeinitRGXTQ2Bridge(void)
{

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMCREATETRANSFERCONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMDESTROYTRANSFERCONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMSUBMITTRANSFER);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMSETTRANSFERCONTEXTPRIORITY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMNOTIFYWRITEOFFSETUPDATE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMSUBMITTRANSFER2);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMGETSHAREDMEMORY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMRELEASESHAREDMEMORY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ2,
				PVRSRV_BRIDGE_RGXTQ2_RGXTDMSETTRANSFERCONTEXTPROPERTY);

	return PVRSRV_OK;
}
