/*******************************************************************************
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
*******************************************************************************/

#include <linux/uaccess.h>

#include "img_defs.h"

#include "rgxtransfer.h"
#include "rgx_tq_shared.h"

#include "common_rgxtq_bridge.h"

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
PVRSRVBridgeRGXCreateTransferContext(IMG_UINT32 ui32DispatchTableEntry,
				     PVRSRV_BRIDGE_IN_RGXCREATETRANSFERCONTEXT *
				     psRGXCreateTransferContextIN,
				     PVRSRV_BRIDGE_OUT_RGXCREATETRANSFERCONTEXT
				     * psRGXCreateTransferContextOUT,
				     CONNECTION_DATA * psConnection)
{
	IMG_BYTE *psFrameworkCmdInt = NULL;
	IMG_HANDLE hPrivData = psRGXCreateTransferContextIN->hPrivData;
	IMG_HANDLE hPrivDataInt = NULL;
	RGX_SERVER_TQ_CONTEXT *psTransferContextInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXCreateTransferContextIN->ui32FrameworkCmdize *
	     sizeof(IMG_BYTE)) + 0;

	if (unlikely
	    (psRGXCreateTransferContextIN->ui32FrameworkCmdize >
	     RGXFWIF_RF_CMD_SIZE))
	{
		psRGXCreateTransferContextOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXCreateTransferContext_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXCreateTransferContextIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psRGXCreateTransferContextIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXCreateTransferContextOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXCreateTransferContext_exit;
			}
		}
	}

	if (psRGXCreateTransferContextIN->ui32FrameworkCmdize != 0)
	{
		psFrameworkCmdInt =
		    (IMG_BYTE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				  ui32NextOffset);
		ui32NextOffset +=
		    psRGXCreateTransferContextIN->ui32FrameworkCmdize *
		    sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXCreateTransferContextIN->ui32FrameworkCmdize *
	    sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, psFrameworkCmdInt,
		     (const void __user *)psRGXCreateTransferContextIN->
		     psFrameworkCmd,
		     psRGXCreateTransferContextIN->ui32FrameworkCmdize *
		     sizeof(IMG_BYTE)) != PVRSRV_OK)
		{
			psRGXCreateTransferContextOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXCreateTransferContext_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXCreateTransferContextOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&hPrivDataInt,
				       hPrivData,
				       PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
				       IMG_TRUE);
	if (unlikely(psRGXCreateTransferContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateTransferContext_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXCreateTransferContextOUT->eError =
	    PVRSRVRGXCreateTransferContextKM(psConnection,
					     OSGetDevData(psConnection),
					     psRGXCreateTransferContextIN->
					     ui32Priority,
					     psRGXCreateTransferContextIN->
					     ui32FrameworkCmdize,
					     psFrameworkCmdInt, hPrivDataInt,
					     psRGXCreateTransferContextIN->
					     ui32PackedCCBSizeU8888,
					     &psTransferContextInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXCreateTransferContextOUT->eError != PVRSRV_OK))
	{
		goto RGXCreateTransferContext_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXCreateTransferContextOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXCreateTransferContextOUT->
				      hTransferContext,
				      (void *)psTransferContextInt,
				      PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      PVRSRVRGXDestroyTransferContextKM);
	if (unlikely(psRGXCreateTransferContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateTransferContext_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

 RGXCreateTransferContext_exit:

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

	if (psRGXCreateTransferContextOUT->eError != PVRSRV_OK)
	{
		if (psTransferContextInt)
		{
			PVRSRVRGXDestroyTransferContextKM(psTransferContextInt);
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
PVRSRVBridgeRGXDestroyTransferContext(IMG_UINT32 ui32DispatchTableEntry,
				      PVRSRV_BRIDGE_IN_RGXDESTROYTRANSFERCONTEXT
				      * psRGXDestroyTransferContextIN,
				      PVRSRV_BRIDGE_OUT_RGXDESTROYTRANSFERCONTEXT
				      * psRGXDestroyTransferContextOUT,
				      CONNECTION_DATA * psConnection)
{

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXDestroyTransferContextOUT->eError =
	    PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					(IMG_HANDLE)
					psRGXDestroyTransferContextIN->
					hTransferContext,
					PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT);
	if (unlikely
	    ((psRGXDestroyTransferContextOUT->eError != PVRSRV_OK)
	     && (psRGXDestroyTransferContextOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "PVRSRVBridgeRGXDestroyTransferContext: %s",
			 PVRSRVGetErrorString(psRGXDestroyTransferContextOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXDestroyTransferContext_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

 RGXDestroyTransferContext_exit:

	return 0;
}

#if defined(SUPPORT_SERVER_SYNC_IMPL)
static IMG_INT
PVRSRVBridgeRGXSubmitTransfer(IMG_UINT32 ui32DispatchTableEntry,
			      PVRSRV_BRIDGE_IN_RGXSUBMITTRANSFER *
			      psRGXSubmitTransferIN,
			      PVRSRV_BRIDGE_OUT_RGXSUBMITTRANSFER *
			      psRGXSubmitTransferOUT,
			      CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hTransferContext = psRGXSubmitTransferIN->hTransferContext;
	RGX_SERVER_TQ_CONTEXT *psTransferContextInt = NULL;
	IMG_UINT32 *ui32ClientFenceCountInt = NULL;
	SYNC_PRIMITIVE_BLOCK ***psFenceUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE **hFenceUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 **ui32FenceSyncOffsetInt = NULL;
	IMG_UINT32 **ui32FenceValueInt = NULL;
	IMG_UINT32 *ui32ClientUpdateCountInt = NULL;
	SYNC_PRIMITIVE_BLOCK ***psUpdateUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE **hUpdateUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 **ui32UpdateSyncOffsetInt = NULL;
	IMG_UINT32 **ui32UpdateValueInt = NULL;
	IMG_UINT32 *ui32ServerSyncCountInt = NULL;
	IMG_UINT32 **ui32ServerSyncFlagsInt = NULL;
	SERVER_SYNC_PRIMITIVE ***psServerSyncInt = NULL;
	IMG_HANDLE **hServerSyncInt2 = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_UINT32 *ui32CommandSizeInt = NULL;
	IMG_UINT8 **ui8FWCommandInt = NULL;
	IMG_UINT32 *ui32TQPrepareFlagsInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR **psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
	IMG_BYTE *pArrayArgsBuffer2 = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(PMR *)) +
	    (psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) + 0;
	IMG_UINT32 ui32BufferSize2 = 0;
	IMG_UINT32 ui32NextOffset2 = 0;

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{

		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK **);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_HANDLE **);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK **);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_HANDLE **);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(SERVER_SYNC_PRIMITIVE **);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_HANDLE **);
		ui32BufferSize +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT8 *);
	}

	if (unlikely
	    (psRGXSubmitTransferIN->ui32PrepareCount >
	     TQ_MAX_PREPARES_PER_SUBMIT))
	{
		psRGXSubmitTransferOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXSubmitTransfer_exit;
	}

	if (unlikely
	    (psRGXSubmitTransferIN->ui32SyncPMRCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXSubmitTransferOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXSubmitTransfer_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXSubmitTransferIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psRGXSubmitTransferIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXSubmitTransfer_exit;
			}
		}
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32ClientFenceCountInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientFenceCountInt,
		     (const void __user *)psRGXSubmitTransferIN->
		     pui32ClientFenceCount,
		     psRGXSubmitTransferIN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer_exit;
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning psFenceUFOSyncPrimBlockInt to the right offset in the pool buffer for first dimension */
		psFenceUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK ***) (((IMG_UINT8 *) pArrayArgsBuffer)
						+ ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK **);
		/* Assigning hFenceUFOSyncPrimBlockInt2 to the right offset in the pool buffer for first dimension */
		hFenceUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_HANDLE);
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning ui32FenceSyncOffsetInt to the right offset in the pool buffer for first dimension */
		ui32FenceSyncOffsetInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning ui32FenceValueInt to the right offset in the pool buffer for first dimension */
		ui32FenceValueInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32ClientUpdateCountInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientUpdateCountInt,
		     (const void __user *)psRGXSubmitTransferIN->
		     pui32ClientUpdateCount,
		     psRGXSubmitTransferIN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer_exit;
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning psUpdateUFOSyncPrimBlockInt to the right offset in the pool buffer for first dimension */
		psUpdateUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK ***) (((IMG_UINT8 *) pArrayArgsBuffer)
						+ ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK **);
		/* Assigning hUpdateUFOSyncPrimBlockInt2 to the right offset in the pool buffer for first dimension */
		hUpdateUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_HANDLE);
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning ui32UpdateSyncOffsetInt to the right offset in the pool buffer for first dimension */
		ui32UpdateSyncOffsetInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning ui32UpdateValueInt to the right offset in the pool buffer for first dimension */
		ui32UpdateValueInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32ServerSyncCountInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ServerSyncCountInt,
		     (const void __user *)psRGXSubmitTransferIN->
		     pui32ServerSyncCount,
		     psRGXSubmitTransferIN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer_exit;
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning ui32ServerSyncFlagsInt to the right offset in the pool buffer for first dimension */
		ui32ServerSyncFlagsInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning psServerSyncInt to the right offset in the pool buffer for first dimension */
		psServerSyncInt =
		    (SERVER_SYNC_PRIMITIVE
		     ***) (((IMG_UINT8 *) pArrayArgsBuffer) + ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(SERVER_SYNC_PRIMITIVE **);
		/* Assigning hServerSyncInt2 to the right offset in the pool buffer for first dimension */
		hServerSyncInt2 =
		    (IMG_HANDLE **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_HANDLE);
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
		     (const void __user *)psRGXSubmitTransferIN->
		     puiUpdateFenceName,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceNameInt)[(PVRSRV_SYNC_NAME_LENGTH *
					sizeof(IMG_CHAR)) - 1] = '\0';
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32CommandSizeInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32CommandSizeInt,
		     (const void __user *)psRGXSubmitTransferIN->
		     pui32CommandSize,
		     psRGXSubmitTransferIN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer_exit;
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		/* Assigning ui8FWCommandInt to the right offset in the pool buffer for first dimension */
		ui8FWCommandInt =
		    (IMG_UINT8 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT8 *);
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		ui32TQPrepareFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransferIN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32TQPrepareFlagsInt,
		     (const void __user *)psRGXSubmitTransferIN->
		     pui32TQPrepareFlags,
		     psRGXSubmitTransferIN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer_exit;
		}
	}
	if (psRGXSubmitTransferIN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32SyncPMRCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32SyncPMRFlagsInt,
		     (const void __user *)psRGXSubmitTransferIN->
		     pui32SyncPMRFlags,
		     psRGXSubmitTransferIN->ui32SyncPMRCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer_exit;
		}
	}
	if (psRGXSubmitTransferIN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt =
		    (PMR **) (((IMG_UINT8 *) pArrayArgsBuffer) +
			      ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(PMR *);
		hSyncPMRsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransferIN->ui32SyncPMRCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXSubmitTransferIN->ui32SyncPMRCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hSyncPMRsInt2,
		     (const void __user *)psRGXSubmitTransferIN->phSyncPMRs,
		     psRGXSubmitTransferIN->ui32SyncPMRCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer_exit;
		}
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			ui32BufferSize2 +=
			    ui32ClientFenceCountInt[i] *
			    sizeof(SYNC_PRIMITIVE_BLOCK *);
			ui32BufferSize2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE *);
			ui32BufferSize2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32ClientUpdateCountInt[i] *
			    sizeof(SYNC_PRIMITIVE_BLOCK *);
			ui32BufferSize2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE *);
			ui32BufferSize2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32ServerSyncCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32ServerSyncCountInt[i] *
			    sizeof(SERVER_SYNC_PRIMITIVE *);
			ui32BufferSize2 +=
			    ui32ServerSyncCountInt[i] * sizeof(IMG_HANDLE *);
			ui32BufferSize2 +=
			    ui32CommandSizeInt[i] * sizeof(IMG_UINT8);
		}
	}

	if (ui32BufferSize2 != 0)
	{
		pArrayArgsBuffer2 = OSAllocMemNoStats(ui32BufferSize2);

		if (!pArrayArgsBuffer2)
		{
			psRGXSubmitTransferOUT->eError =
			    PVRSRV_ERROR_OUT_OF_MEMORY;
			goto RGXSubmitTransfer_exit;
		}
	}

	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			if (ui32ClientFenceCountInt[i] > PVRSRV_MAX_SYNC_PRIMS)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
				goto RGXSubmitTransfer_exit;
			}

			/* Assigning each psFenceUFOSyncPrimBlockInt to the right offset in the pool buffer (this is the second dimension) */
			psFenceUFOSyncPrimBlockInt[i] =
			    (SYNC_PRIMITIVE_BLOCK
			     **) (((IMG_UINT8 *) pArrayArgsBuffer2) +
				  ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientFenceCountInt[i] *
			    sizeof(SYNC_PRIMITIVE_BLOCK *);
			/* Assigning each hFenceUFOSyncPrimBlockInt2 to the right offset in the pool buffer (this is the second dimension) */
			hFenceUFOSyncPrimBlockInt2[i] =
			    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE);
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Assigning each ui32FenceSyncOffsetInt to the right offset in the pool buffer (this is the second dimension) */
			ui32FenceSyncOffsetInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Assigning each ui32FenceValueInt to the right offset in the pool buffer (this is the second dimension) */
			ui32FenceValueInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			if (ui32ClientUpdateCountInt[i] > PVRSRV_MAX_SYNC_PRIMS)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
				goto RGXSubmitTransfer_exit;
			}

			/* Assigning each psUpdateUFOSyncPrimBlockInt to the right offset in the pool buffer (this is the second dimension) */
			psUpdateUFOSyncPrimBlockInt[i] =
			    (SYNC_PRIMITIVE_BLOCK
			     **) (((IMG_UINT8 *) pArrayArgsBuffer2) +
				  ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientUpdateCountInt[i] *
			    sizeof(SYNC_PRIMITIVE_BLOCK *);
			/* Assigning each hUpdateUFOSyncPrimBlockInt2 to the right offset in the pool buffer (this is the second dimension) */
			hUpdateUFOSyncPrimBlockInt2[i] =
			    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE);
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Assigning each ui32UpdateSyncOffsetInt to the right offset in the pool buffer (this is the second dimension) */
			ui32UpdateSyncOffsetInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Assigning each ui32UpdateValueInt to the right offset in the pool buffer (this is the second dimension) */
			ui32UpdateValueInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			if (ui32ServerSyncCountInt[i] > PVRSRV_MAX_SYNC_PRIMS)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
				goto RGXSubmitTransfer_exit;
			}

			/* Assigning each ui32ServerSyncFlagsInt to the right offset in the pool buffer (this is the second dimension) */
			ui32ServerSyncFlagsInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ServerSyncCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Assigning each psServerSyncInt to the right offset in the pool buffer (this is the second dimension) */
			psServerSyncInt[i] =
			    (SERVER_SYNC_PRIMITIVE
			     **) (((IMG_UINT8 *) pArrayArgsBuffer2) +
				  ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ServerSyncCountInt[i] *
			    sizeof(SERVER_SYNC_PRIMITIVE *);
			/* Assigning each hServerSyncInt2 to the right offset in the pool buffer (this is the second dimension) */
			hServerSyncInt2[i] =
			    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ServerSyncCountInt[i] * sizeof(IMG_HANDLE);
		}
	}
	if (psRGXSubmitTransferIN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			if (ui32CommandSizeInt[i] >
			    RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
				goto RGXSubmitTransfer_exit;
			}

			/* Assigning each ui8FWCommandInt to the right offset in the pool buffer (this is the second dimension) */
			ui8FWCommandInt[i] =
			    (IMG_UINT8 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					   ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32CommandSizeInt[i] * sizeof(IMG_UINT8);
		}
	}

	{
		IMG_UINT32 i;
		IMG_HANDLE **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     phFenceUFOSyncPrimBlock[i],
			     sizeof(IMG_HANDLE **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (hFenceUFOSyncPrimBlockInt2[i]),
				     (const void __user *)psPtr,
				     (ui32ClientFenceCountInt[i] *
				      sizeof(IMG_HANDLE))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     pui32FenceSyncOffset[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32FenceSyncOffsetInt[i]),
				     (const void __user *)psPtr,
				     (ui32ClientFenceCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     pui32FenceValue[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32FenceValueInt[i]),
				     (const void __user *)psPtr,
				     (ui32ClientFenceCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_HANDLE **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     phUpdateUFOSyncPrimBlock[i],
			     sizeof(IMG_HANDLE **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (hUpdateUFOSyncPrimBlockInt2[i]),
				     (const void __user *)psPtr,
				     (ui32ClientUpdateCountInt[i] *
				      sizeof(IMG_HANDLE))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     pui32UpdateSyncOffset[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32UpdateSyncOffsetInt[i]),
				     (const void __user *)psPtr,
				     (ui32ClientUpdateCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     pui32UpdateValue[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32UpdateValueInt[i]),
				     (const void __user *)psPtr,
				     (ui32ClientUpdateCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     pui32ServerSyncFlags[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32ServerSyncCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32ServerSyncFlagsInt[i]),
				     (const void __user *)psPtr,
				     (ui32ServerSyncCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_HANDLE **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     phServerSync[i],
			     sizeof(IMG_HANDLE **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32ServerSyncCountInt[i] * sizeof(IMG_HANDLE)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (hServerSyncInt2[i]),
				     (const void __user *)psPtr,
				     (ui32ServerSyncCountInt[i] *
				      sizeof(IMG_HANDLE))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT8 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransferIN->
			     pui8FWCommand[i],
			     sizeof(IMG_UINT8 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransferOUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer_exit;
			}

			/* Copy the data over */
			if ((ui32CommandSizeInt[i] * sizeof(IMG_UINT8)) > 0)
			{
				if (OSCopyFromUser
				    (NULL, (ui8FWCommandInt[i]),
				     (const void __user *)psPtr,
				     (ui32CommandSizeInt[i] *
				      sizeof(IMG_UINT8))) != PVRSRV_OK)
				{
					psRGXSubmitTransferOUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXSubmitTransferOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psTransferContextInt,
				       hTransferContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXSubmitTransferOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXSubmitTransfer_exit;
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ClientFenceCountInt[i]; j++)
			{
				/* Look up the address from the handle */
				psRGXSubmitTransferOUT->eError =
				    PVRSRVLookupHandleUnlocked(psConnection->
							       psHandleBase,
							       (void **)
							       &psFenceUFOSyncPrimBlockInt
							       [i][j],
							       hFenceUFOSyncPrimBlockInt2
							       [i][j],
							       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
							       IMG_TRUE);
				if (unlikely
				    (psRGXSubmitTransferOUT->eError !=
				     PVRSRV_OK))
				{
					UnlockHandle(psConnection->
						     psHandleBase);
					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ClientUpdateCountInt[i]; j++)
			{
				/* Look up the address from the handle */
				psRGXSubmitTransferOUT->eError =
				    PVRSRVLookupHandleUnlocked(psConnection->
							       psHandleBase,
							       (void **)
							       &psUpdateUFOSyncPrimBlockInt
							       [i][j],
							       hUpdateUFOSyncPrimBlockInt2
							       [i][j],
							       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
							       IMG_TRUE);
				if (unlikely
				    (psRGXSubmitTransferOUT->eError !=
				     PVRSRV_OK))
				{
					UnlockHandle(psConnection->
						     psHandleBase);
					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ServerSyncCountInt[i]; j++)
			{
				/* Look up the address from the handle */
				psRGXSubmitTransferOUT->eError =
				    PVRSRVLookupHandleUnlocked(psConnection->
							       psHandleBase,
							       (void **)
							       &psServerSyncInt
							       [i][j],
							       hServerSyncInt2
							       [i][j],
							       PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE,
							       IMG_TRUE);
				if (unlikely
				    (psRGXSubmitTransferOUT->eError !=
				     PVRSRV_OK))
				{
					UnlockHandle(psConnection->
						     psHandleBase);
					goto RGXSubmitTransfer_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransferIN->ui32SyncPMRCount; i++)
		{
			/* Look up the address from the handle */
			psRGXSubmitTransferOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psSyncPMRsInt[i],
						       hSyncPMRsInt2[i],
						       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
						       IMG_TRUE);
			if (unlikely
			    (psRGXSubmitTransferOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXSubmitTransfer_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXSubmitTransferOUT->eError =
	    PVRSRVRGXSubmitTransferKM(psTransferContextInt,
				      psRGXSubmitTransferIN->
				      ui32ClientCacheOpSeqNum,
				      psRGXSubmitTransferIN->ui32PrepareCount,
				      ui32ClientFenceCountInt,
				      psFenceUFOSyncPrimBlockInt,
				      ui32FenceSyncOffsetInt, ui32FenceValueInt,
				      ui32ClientUpdateCountInt,
				      psUpdateUFOSyncPrimBlockInt,
				      ui32UpdateSyncOffsetInt,
				      ui32UpdateValueInt,
				      ui32ServerSyncCountInt,
				      ui32ServerSyncFlagsInt, psServerSyncInt,
				      psRGXSubmitTransferIN->hCheckFenceFD,
				      psRGXSubmitTransferIN->h2DUpdateTimeline,
				      &psRGXSubmitTransferOUT->h2DUpdateFence,
				      psRGXSubmitTransferIN->h3DUpdateTimeline,
				      &psRGXSubmitTransferOUT->h3DUpdateFence,
				      uiUpdateFenceNameInt, ui32CommandSizeInt,
				      ui8FWCommandInt, ui32TQPrepareFlagsInt,
				      psRGXSubmitTransferIN->ui32ExtJobRef,
				      psRGXSubmitTransferIN->ui32SyncPMRCount,
				      ui32SyncPMRFlagsInt, psSyncPMRsInt);

 RGXSubmitTransfer_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psTransferContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT);
	}

	if (hFenceUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ClientFenceCountInt[i]; j++)
			{

				/* Unreference the previously looked up handle */
				if (hFenceUFOSyncPrimBlockInt2[i][j])
				{
					PVRSRVReleaseHandleUnlocked
					    (psConnection->psHandleBase,
					     hFenceUFOSyncPrimBlockInt2[i][j],
					     PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
				}
			}
		}
	}

	if (hUpdateUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ClientUpdateCountInt[i]; j++)
			{

				/* Unreference the previously looked up handle */
				if (hUpdateUFOSyncPrimBlockInt2[i][j])
				{
					PVRSRVReleaseHandleUnlocked
					    (psConnection->psHandleBase,
					     hUpdateUFOSyncPrimBlockInt2[i][j],
					     PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
				}
			}
		}
	}

	if (hServerSyncInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransferIN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ServerSyncCountInt[i]; j++)
			{

				/* Unreference the previously looked up handle */
				if (hServerSyncInt2[i][j])
				{
					PVRSRVReleaseHandleUnlocked
					    (psConnection->psHandleBase,
					     hServerSyncInt2[i][j],
					     PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
				}
			}
		}
	}

	if (hSyncPMRsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransferIN->ui32SyncPMRCount; i++)
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

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize2 == ui32NextOffset2);

	if (pArrayArgsBuffer2)
		OSFreeMemNoStats(pArrayArgsBuffer2);

	return 0;
}

#else
#define PVRSRVBridgeRGXSubmitTransfer NULL
#endif

static IMG_INT
PVRSRVBridgeRGXSetTransferContextPriority(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_RGXSETTRANSFERCONTEXTPRIORITY
					  * psRGXSetTransferContextPriorityIN,
					  PVRSRV_BRIDGE_OUT_RGXSETTRANSFERCONTEXTPRIORITY
					  * psRGXSetTransferContextPriorityOUT,
					  CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hTransferContext =
	    psRGXSetTransferContextPriorityIN->hTransferContext;
	RGX_SERVER_TQ_CONTEXT *psTransferContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXSetTransferContextPriorityOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psTransferContextInt,
				       hTransferContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXSetTransferContextPriorityOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXSetTransferContextPriority_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXSetTransferContextPriorityOUT->eError =
	    PVRSRVRGXSetTransferContextPriorityKM(psConnection,
						  OSGetDevData(psConnection),
						  psTransferContextInt,
						  psRGXSetTransferContextPriorityIN->
						  ui32Priority);

 RGXSetTransferContextPriority_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psTransferContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

#if !defined(SUPPORT_SERVER_SYNC_IMPL)
static IMG_INT
PVRSRVBridgeRGXSubmitTransfer2(IMG_UINT32 ui32DispatchTableEntry,
			       PVRSRV_BRIDGE_IN_RGXSUBMITTRANSFER2 *
			       psRGXSubmitTransfer2IN,
			       PVRSRV_BRIDGE_OUT_RGXSUBMITTRANSFER2 *
			       psRGXSubmitTransfer2OUT,
			       CONNECTION_DATA * psConnection)
{
	IMG_HANDLE hTransferContext = psRGXSubmitTransfer2IN->hTransferContext;
	RGX_SERVER_TQ_CONTEXT *psTransferContextInt = NULL;
	IMG_UINT32 *ui32ClientFenceCountInt = NULL;
	SYNC_PRIMITIVE_BLOCK ***psFenceUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE **hFenceUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 **ui32FenceSyncOffsetInt = NULL;
	IMG_UINT32 **ui32FenceValueInt = NULL;
	IMG_UINT32 *ui32ClientUpdateCountInt = NULL;
	SYNC_PRIMITIVE_BLOCK ***psUpdateUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE **hUpdateUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 **ui32UpdateSyncOffsetInt = NULL;
	IMG_UINT32 **ui32UpdateValueInt = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_UINT32 *ui32CommandSizeInt = NULL;
	IMG_UINT8 **ui8FWCommandInt = NULL;
	IMG_UINT32 *ui32TQPrepareFlagsInt = NULL;
	IMG_UINT32 *ui32SyncPMRFlagsInt = NULL;
	PMR **psSyncPMRsInt = NULL;
	IMG_HANDLE *hSyncPMRsInt2 = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
	IMG_BYTE *pArrayArgsBuffer2 = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXSubmitTransfer2IN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransfer2IN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (psRGXSubmitTransfer2IN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransfer2IN->ui32PrepareCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransfer2IN->ui32SyncPMRCount * sizeof(IMG_UINT32)) +
	    (psRGXSubmitTransfer2IN->ui32SyncPMRCount * sizeof(PMR *)) +
	    (psRGXSubmitTransfer2IN->ui32SyncPMRCount * sizeof(IMG_HANDLE)) + 0;
	IMG_UINT32 ui32BufferSize2 = 0;
	IMG_UINT32 ui32NextOffset2 = 0;

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{

		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK **);
		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_HANDLE **);
		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK **);
		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_HANDLE **);
		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
		ui32BufferSize +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT8 *);
	}

	if (unlikely
	    (psRGXSubmitTransfer2IN->ui32PrepareCount >
	     TQ_MAX_PREPARES_PER_SUBMIT))
	{
		psRGXSubmitTransfer2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXSubmitTransfer2_exit;
	}

	if (unlikely
	    (psRGXSubmitTransfer2IN->ui32SyncPMRCount > PVRSRV_MAX_SYNC_PRIMS))
	{
		psRGXSubmitTransfer2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXSubmitTransfer2_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXSubmitTransfer2IN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) psRGXSubmitTransfer2IN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXSubmitTransfer2_exit;
			}
		}
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		ui32ClientFenceCountInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransfer2IN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientFenceCountInt,
		     (const void __user *)psRGXSubmitTransfer2IN->
		     pui32ClientFenceCount,
		     psRGXSubmitTransfer2IN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer2_exit;
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		/* Assigning psFenceUFOSyncPrimBlockInt to the right offset in the pool buffer for first dimension */
		psFenceUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK ***) (((IMG_UINT8 *) pArrayArgsBuffer)
						+ ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK **);
		/* Assigning hFenceUFOSyncPrimBlockInt2 to the right offset in the pool buffer for first dimension */
		hFenceUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_HANDLE);
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		/* Assigning ui32FenceSyncOffsetInt to the right offset in the pool buffer for first dimension */
		ui32FenceSyncOffsetInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		/* Assigning ui32FenceValueInt to the right offset in the pool buffer for first dimension */
		ui32FenceValueInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		ui32ClientUpdateCountInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransfer2IN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientUpdateCountInt,
		     (const void __user *)psRGXSubmitTransfer2IN->
		     pui32ClientUpdateCount,
		     psRGXSubmitTransfer2IN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer2_exit;
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		/* Assigning psUpdateUFOSyncPrimBlockInt to the right offset in the pool buffer for first dimension */
		psUpdateUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK ***) (((IMG_UINT8 *) pArrayArgsBuffer)
						+ ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK **);
		/* Assigning hUpdateUFOSyncPrimBlockInt2 to the right offset in the pool buffer for first dimension */
		hUpdateUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_HANDLE);
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		/* Assigning ui32UpdateSyncOffsetInt to the right offset in the pool buffer for first dimension */
		ui32UpdateSyncOffsetInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		/* Assigning ui32UpdateValueInt to the right offset in the pool buffer for first dimension */
		ui32UpdateValueInt =
		    (IMG_UINT32 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				     ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32 *);
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
		     (const void __user *)psRGXSubmitTransfer2IN->
		     puiUpdateFenceName,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer2_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceNameInt)[(PVRSRV_SYNC_NAME_LENGTH *
					sizeof(IMG_CHAR)) - 1] = '\0';
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		ui32CommandSizeInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransfer2IN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32CommandSizeInt,
		     (const void __user *)psRGXSubmitTransfer2IN->
		     pui32CommandSize,
		     psRGXSubmitTransfer2IN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer2_exit;
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		/* Assigning ui8FWCommandInt to the right offset in the pool buffer for first dimension */
		ui8FWCommandInt =
		    (IMG_UINT8 **) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT8 *);
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		ui32TQPrepareFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32PrepareCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransfer2IN->ui32PrepareCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32TQPrepareFlagsInt,
		     (const void __user *)psRGXSubmitTransfer2IN->
		     pui32TQPrepareFlags,
		     psRGXSubmitTransfer2IN->ui32PrepareCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer2_exit;
		}
	}
	if (psRGXSubmitTransfer2IN->ui32SyncPMRCount != 0)
	{
		ui32SyncPMRFlagsInt =
		    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32SyncPMRCount *
		    sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXSubmitTransfer2IN->ui32SyncPMRCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32SyncPMRFlagsInt,
		     (const void __user *)psRGXSubmitTransfer2IN->
		     pui32SyncPMRFlags,
		     psRGXSubmitTransfer2IN->ui32SyncPMRCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer2_exit;
		}
	}
	if (psRGXSubmitTransfer2IN->ui32SyncPMRCount != 0)
	{
		psSyncPMRsInt =
		    (PMR **) (((IMG_UINT8 *) pArrayArgsBuffer) +
			      ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32SyncPMRCount * sizeof(PMR *);
		hSyncPMRsInt2 =
		    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer) +
				    ui32NextOffset);
		ui32NextOffset +=
		    psRGXSubmitTransfer2IN->ui32SyncPMRCount *
		    sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXSubmitTransfer2IN->ui32SyncPMRCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hSyncPMRsInt2,
		     (const void __user *)psRGXSubmitTransfer2IN->phSyncPMRs,
		     psRGXSubmitTransfer2IN->ui32SyncPMRCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXSubmitTransfer2_exit;
		}
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			ui32BufferSize2 +=
			    ui32ClientFenceCountInt[i] *
			    sizeof(SYNC_PRIMITIVE_BLOCK *);
			ui32BufferSize2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE *);
			ui32BufferSize2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32ClientUpdateCountInt[i] *
			    sizeof(SYNC_PRIMITIVE_BLOCK *);
			ui32BufferSize2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE *);
			ui32BufferSize2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);
			ui32BufferSize2 +=
			    ui32CommandSizeInt[i] * sizeof(IMG_UINT8);
		}
	}

	if (ui32BufferSize2 != 0)
	{
		pArrayArgsBuffer2 = OSAllocMemNoStats(ui32BufferSize2);

		if (!pArrayArgsBuffer2)
		{
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRV_ERROR_OUT_OF_MEMORY;
			goto RGXSubmitTransfer2_exit;
		}
	}

	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			if (ui32ClientFenceCountInt[i] > PVRSRV_MAX_SYNC_PRIMS)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
				goto RGXSubmitTransfer2_exit;
			}

			/* Assigning each psFenceUFOSyncPrimBlockInt to the right offset in the pool buffer (this is the second dimension) */
			psFenceUFOSyncPrimBlockInt[i] =
			    (SYNC_PRIMITIVE_BLOCK
			     **) (((IMG_UINT8 *) pArrayArgsBuffer2) +
				  ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientFenceCountInt[i] *
			    sizeof(SYNC_PRIMITIVE_BLOCK *);
			/* Assigning each hFenceUFOSyncPrimBlockInt2 to the right offset in the pool buffer (this is the second dimension) */
			hFenceUFOSyncPrimBlockInt2[i] =
			    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE);
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Assigning each ui32FenceSyncOffsetInt to the right offset in the pool buffer (this is the second dimension) */
			ui32FenceSyncOffsetInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Assigning each ui32FenceValueInt to the right offset in the pool buffer (this is the second dimension) */
			ui32FenceValueInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			if (ui32ClientUpdateCountInt[i] > PVRSRV_MAX_SYNC_PRIMS)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
				goto RGXSubmitTransfer2_exit;
			}

			/* Assigning each psUpdateUFOSyncPrimBlockInt to the right offset in the pool buffer (this is the second dimension) */
			psUpdateUFOSyncPrimBlockInt[i] =
			    (SYNC_PRIMITIVE_BLOCK
			     **) (((IMG_UINT8 *) pArrayArgsBuffer2) +
				  ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientUpdateCountInt[i] *
			    sizeof(SYNC_PRIMITIVE_BLOCK *);
			/* Assigning each hUpdateUFOSyncPrimBlockInt2 to the right offset in the pool buffer (this is the second dimension) */
			hUpdateUFOSyncPrimBlockInt2[i] =
			    (IMG_HANDLE *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE);
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Assigning each ui32UpdateSyncOffsetInt to the right offset in the pool buffer (this is the second dimension) */
			ui32UpdateSyncOffsetInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Assigning each ui32UpdateValueInt to the right offset in the pool buffer (this is the second dimension) */
			ui32UpdateValueInt[i] =
			    (IMG_UINT32 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					    ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32);
		}
	}
	if (psRGXSubmitTransfer2IN->ui32PrepareCount != 0)
	{
		IMG_UINT32 i;
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			if (ui32CommandSizeInt[i] >
			    RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
				goto RGXSubmitTransfer2_exit;
			}

			/* Assigning each ui8FWCommandInt to the right offset in the pool buffer (this is the second dimension) */
			ui8FWCommandInt[i] =
			    (IMG_UINT8 *) (((IMG_UINT8 *) pArrayArgsBuffer2) +
					   ui32NextOffset2);
			ui32NextOffset2 +=
			    ui32CommandSizeInt[i] * sizeof(IMG_UINT8);
		}
	}

	{
		IMG_UINT32 i;
		IMG_HANDLE **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransfer2IN->
			     phFenceUFOSyncPrimBlock[i],
			     sizeof(IMG_HANDLE **)) != PVRSRV_OK)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer2_exit;
			}

			/* Copy the data over */
			if ((ui32ClientFenceCountInt[i] * sizeof(IMG_HANDLE)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (hFenceUFOSyncPrimBlockInt2[i]),
				     (const void __user *)psPtr,
				     (ui32ClientFenceCountInt[i] *
				      sizeof(IMG_HANDLE))) != PVRSRV_OK)
				{
					psRGXSubmitTransfer2OUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransfer2IN->
			     pui32FenceSyncOffset[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer2_exit;
			}

			/* Copy the data over */
			if ((ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32FenceSyncOffsetInt[i]),
				     (const void __user *)psPtr,
				     (ui32ClientFenceCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransfer2OUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransfer2IN->
			     pui32FenceValue[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer2_exit;
			}

			/* Copy the data over */
			if ((ui32ClientFenceCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32FenceValueInt[i]),
				     (const void __user *)psPtr,
				     (ui32ClientFenceCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransfer2OUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_HANDLE **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransfer2IN->
			     phUpdateUFOSyncPrimBlock[i],
			     sizeof(IMG_HANDLE **)) != PVRSRV_OK)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer2_exit;
			}

			/* Copy the data over */
			if ((ui32ClientUpdateCountInt[i] * sizeof(IMG_HANDLE)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (hUpdateUFOSyncPrimBlockInt2[i]),
				     (const void __user *)psPtr,
				     (ui32ClientUpdateCountInt[i] *
				      sizeof(IMG_HANDLE))) != PVRSRV_OK)
				{
					psRGXSubmitTransfer2OUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransfer2IN->
			     pui32UpdateSyncOffset[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer2_exit;
			}

			/* Copy the data over */
			if ((ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32UpdateSyncOffsetInt[i]),
				     (const void __user *)psPtr,
				     (ui32ClientUpdateCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransfer2OUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT32 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransfer2IN->
			     pui32UpdateValue[i],
			     sizeof(IMG_UINT32 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer2_exit;
			}

			/* Copy the data over */
			if ((ui32ClientUpdateCountInt[i] * sizeof(IMG_UINT32)) >
			    0)
			{
				if (OSCopyFromUser
				    (NULL, (ui32UpdateValueInt[i]),
				     (const void __user *)psPtr,
				     (ui32ClientUpdateCountInt[i] *
				      sizeof(IMG_UINT32))) != PVRSRV_OK)
				{
					psRGXSubmitTransfer2OUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;
		IMG_UINT8 **psPtr;

		/* Loop over all the pointers in the array copying the data into the kernel */
		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			/* Copy the pointer over from the client side */
			if (OSCopyFromUser
			    (NULL, &psPtr,
			     (const void __user *)&psRGXSubmitTransfer2IN->
			     pui8FWCommand[i],
			     sizeof(IMG_UINT8 **)) != PVRSRV_OK)
			{
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXSubmitTransfer2_exit;
			}

			/* Copy the data over */
			if ((ui32CommandSizeInt[i] * sizeof(IMG_UINT8)) > 0)
			{
				if (OSCopyFromUser
				    (NULL, (ui8FWCommandInt[i]),
				     (const void __user *)psPtr,
				     (ui32CommandSizeInt[i] *
				      sizeof(IMG_UINT8))) != PVRSRV_OK)
				{
					psRGXSubmitTransfer2OUT->eError =
					    PVRSRV_ERROR_INVALID_PARAMS;

					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXSubmitTransfer2OUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psTransferContextInt,
				       hTransferContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXSubmitTransfer2OUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXSubmitTransfer2_exit;
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ClientFenceCountInt[i]; j++)
			{
				/* Look up the address from the handle */
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRVLookupHandleUnlocked(psConnection->
							       psHandleBase,
							       (void **)
							       &psFenceUFOSyncPrimBlockInt
							       [i][j],
							       hFenceUFOSyncPrimBlockInt2
							       [i][j],
							       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
							       IMG_TRUE);
				if (unlikely
				    (psRGXSubmitTransfer2OUT->eError !=
				     PVRSRV_OK))
				{
					UnlockHandle(psConnection->
						     psHandleBase);
					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ClientUpdateCountInt[i]; j++)
			{
				/* Look up the address from the handle */
				psRGXSubmitTransfer2OUT->eError =
				    PVRSRVLookupHandleUnlocked(psConnection->
							       psHandleBase,
							       (void **)
							       &psUpdateUFOSyncPrimBlockInt
							       [i][j],
							       hUpdateUFOSyncPrimBlockInt2
							       [i][j],
							       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
							       IMG_TRUE);
				if (unlikely
				    (psRGXSubmitTransfer2OUT->eError !=
				     PVRSRV_OK))
				{
					UnlockHandle(psConnection->
						     psHandleBase);
					goto RGXSubmitTransfer2_exit;
				}
			}
		}
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransfer2IN->ui32SyncPMRCount; i++)
		{
			/* Look up the address from the handle */
			psRGXSubmitTransfer2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psSyncPMRsInt[i],
						       hSyncPMRsInt2[i],
						       PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
						       IMG_TRUE);
			if (unlikely
			    (psRGXSubmitTransfer2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXSubmitTransfer2_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXSubmitTransfer2OUT->eError =
	    PVRSRVRGXSubmitTransfer2KM(psTransferContextInt,
				       psRGXSubmitTransfer2IN->
				       ui32ClientCacheOpSeqNum,
				       psRGXSubmitTransfer2IN->ui32PrepareCount,
				       ui32ClientFenceCountInt,
				       psFenceUFOSyncPrimBlockInt,
				       ui32FenceSyncOffsetInt,
				       ui32FenceValueInt,
				       ui32ClientUpdateCountInt,
				       psUpdateUFOSyncPrimBlockInt,
				       ui32UpdateSyncOffsetInt,
				       ui32UpdateValueInt,
				       psRGXSubmitTransfer2IN->hCheckFenceFD,
				       psRGXSubmitTransfer2IN->
				       h2DUpdateTimeline,
				       &psRGXSubmitTransfer2OUT->h2DUpdateFence,
				       psRGXSubmitTransfer2IN->
				       h3DUpdateTimeline,
				       &psRGXSubmitTransfer2OUT->h3DUpdateFence,
				       uiUpdateFenceNameInt, ui32CommandSizeInt,
				       ui8FWCommandInt, ui32TQPrepareFlagsInt,
				       psRGXSubmitTransfer2IN->ui32ExtJobRef,
				       psRGXSubmitTransfer2IN->ui32SyncPMRCount,
				       ui32SyncPMRFlagsInt, psSyncPMRsInt);

 RGXSubmitTransfer2_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psTransferContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hTransferContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_TQ_CONTEXT);
	}

	if (hFenceUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ClientFenceCountInt[i]; j++)
			{

				/* Unreference the previously looked up handle */
				if (hFenceUFOSyncPrimBlockInt2[i][j])
				{
					PVRSRVReleaseHandleUnlocked
					    (psConnection->psHandleBase,
					     hFenceUFOSyncPrimBlockInt2[i][j],
					     PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
				}
			}
		}
	}

	if (hUpdateUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransfer2IN->ui32PrepareCount; i++)
		{
			IMG_UINT32 j;
			for (j = 0; j < ui32ClientUpdateCountInt[i]; j++)
			{

				/* Unreference the previously looked up handle */
				if (hUpdateUFOSyncPrimBlockInt2[i][j])
				{
					PVRSRVReleaseHandleUnlocked
					    (psConnection->psHandleBase,
					     hUpdateUFOSyncPrimBlockInt2[i][j],
					     PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
				}
			}
		}
	}

	if (hSyncPMRsInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXSubmitTransfer2IN->ui32SyncPMRCount; i++)
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

	/* Allocated space should be equal to the last updated offset */
	PVR_ASSERT(ui32BufferSize2 == ui32NextOffset2);

	if (pArrayArgsBuffer2)
		OSFreeMemNoStats(pArrayArgsBuffer2);

	return 0;
}

#else
#define PVRSRVBridgeRGXSubmitTransfer2 NULL
#endif

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

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
			      PVRSRV_BRIDGE_RGXTQ_RGXCREATETRANSFERCONTEXT,
			      PVRSRVBridgeRGXCreateTransferContext, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
			      PVRSRV_BRIDGE_RGXTQ_RGXDESTROYTRANSFERCONTEXT,
			      PVRSRVBridgeRGXDestroyTransferContext, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
			      PVRSRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER,
			      PVRSRVBridgeRGXSubmitTransfer, NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
			      PVRSRV_BRIDGE_RGXTQ_RGXSETTRANSFERCONTEXTPRIORITY,
			      PVRSRVBridgeRGXSetTransferContextPriority, NULL,
			      bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
			      PVRSRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER2,
			      PVRSRVBridgeRGXSubmitTransfer2, NULL, bUseLock);

	return PVRSRV_OK;
}

/*
 * Unregister all rgxtq functions with services
 */
PVRSRV_ERROR DeinitRGXTQBridge(void)
{

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
				PVRSRV_BRIDGE_RGXTQ_RGXCREATETRANSFERCONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
				PVRSRV_BRIDGE_RGXTQ_RGXDESTROYTRANSFERCONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
				PVRSRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
				PVRSRV_BRIDGE_RGXTQ_RGXSETTRANSFERCONTEXTPRIORITY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXTQ,
				PVRSRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER2);

	return PVRSRV_OK;
}
