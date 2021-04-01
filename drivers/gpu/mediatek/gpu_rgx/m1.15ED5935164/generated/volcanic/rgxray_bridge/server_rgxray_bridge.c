/*******************************************************************************
@File
@Title          Server bridge for rgxray
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxray
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

#include "rgxray.h"

#include "common_rgxray_bridge.h"

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

static PVRSRV_ERROR _RGXCreateRayContextpsRayContextIntRelease(void *pvData)
{
	PVRSRV_ERROR eError;
	eError = PVRSRVRGXDestroyRayContextKM((RGX_SERVER_RAY_CONTEXT *) pvData);
	return eError;
}

static IMG_INT
PVRSRVBridgeRGXCreateRayContext(IMG_UINT32 ui32DispatchTableEntry,
				IMG_UINT8 * psRGXCreateRayContextIN_UI8,
				IMG_UINT8 * psRGXCreateRayContextOUT_UI8,
				CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXCREATERAYCONTEXT *psRGXCreateRayContextIN =
	    (PVRSRV_BRIDGE_IN_RGXCREATERAYCONTEXT *) IMG_OFFSET_ADDR(psRGXCreateRayContextIN_UI8,
								     0);
	PVRSRV_BRIDGE_OUT_RGXCREATERAYCONTEXT *psRGXCreateRayContextOUT =
	    (PVRSRV_BRIDGE_OUT_RGXCREATERAYCONTEXT *) IMG_OFFSET_ADDR(psRGXCreateRayContextOUT_UI8,
								      0);

	IMG_HANDLE hPrivData = psRGXCreateRayContextIN->hPrivData;
	IMG_HANDLE hPrivDataInt = NULL;
	RGX_SERVER_RAY_CONTEXT *psRayContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXCreateRayContextOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&hPrivDataInt,
				       hPrivData, PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA, IMG_TRUE);
	if (unlikely(psRGXCreateRayContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateRayContext_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXCreateRayContextOUT->eError =
	    PVRSRVRGXCreateRayContextKM(psConnection, OSGetDevNode(psConnection),
					psRGXCreateRayContextIN->ui32ui32Priority,
					hPrivDataInt,
					psRGXCreateRayContextIN->ui32ContextFlags,
					&psRayContextInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXCreateRayContextOUT->eError != PVRSRV_OK))
	{
		goto RGXCreateRayContext_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXCreateRayContextOUT->eError = PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
								     &psRGXCreateRayContextOUT->
								     hRayContext,
								     (void *)psRayContextInt,
								     PVRSRV_HANDLE_TYPE_RGX_SERVER_RAY_CONTEXT,
								     PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
								     (PFN_HANDLE_RELEASE) &
								     _RGXCreateRayContextpsRayContextIntRelease);
	if (unlikely(psRGXCreateRayContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateRayContext_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXCreateRayContext_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (hPrivDataInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hPrivData, PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	if (psRGXCreateRayContextOUT->eError != PVRSRV_OK)
	{
		if (psRayContextInt)
		{
			PVRSRVRGXDestroyRayContextKM(psRayContextInt);
		}
	}

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyRayContext(IMG_UINT32 ui32DispatchTableEntry,
				 IMG_UINT8 * psRGXDestroyRayContextIN_UI8,
				 IMG_UINT8 * psRGXDestroyRayContextOUT_UI8,
				 CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXDESTROYRAYCONTEXT *psRGXDestroyRayContextIN =
	    (PVRSRV_BRIDGE_IN_RGXDESTROYRAYCONTEXT *) IMG_OFFSET_ADDR(psRGXDestroyRayContextIN_UI8,
								      0);
	PVRSRV_BRIDGE_OUT_RGXDESTROYRAYCONTEXT *psRGXDestroyRayContextOUT =
	    (PVRSRV_BRIDGE_OUT_RGXDESTROYRAYCONTEXT *)
	    IMG_OFFSET_ADDR(psRGXDestroyRayContextOUT_UI8, 0);

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXDestroyRayContextOUT->eError =
	    PVRSRVReleaseHandleStagedUnlock(psConnection->psHandleBase,
					    (IMG_HANDLE) psRGXDestroyRayContextIN->hRayContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_RAY_CONTEXT);
	if (unlikely((psRGXDestroyRayContextOUT->eError != PVRSRV_OK) &&
		     (psRGXDestroyRayContextOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: %s",
			 __func__, PVRSRVGetErrorString(psRGXDestroyRayContextOUT->eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXDestroyRayContext_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXDestroyRayContext_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXKickRDM(IMG_UINT32 ui32DispatchTableEntry,
		       IMG_UINT8 * psRGXKickRDMIN_UI8,
		       IMG_UINT8 * psRGXKickRDMOUT_UI8, CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXKICKRDM *psRGXKickRDMIN =
	    (PVRSRV_BRIDGE_IN_RGXKICKRDM *) IMG_OFFSET_ADDR(psRGXKickRDMIN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXKICKRDM *psRGXKickRDMOUT =
	    (PVRSRV_BRIDGE_OUT_RGXKICKRDM *) IMG_OFFSET_ADDR(psRGXKickRDMOUT_UI8, 0);

	IMG_HANDLE hRayContext = psRGXKickRDMIN->hRayContext;
	RGX_SERVER_RAY_CONTEXT *psRayContextInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClientUpdateUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClientUpdateUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32ClientUpdateOffsetInt = NULL;
	IMG_UINT32 *ui32ClientUpdateValueInt = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_BYTE *ui8DMCmdInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize =
	    (psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    (psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_HANDLE)) +
	    (psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32)) +
	    (psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32)) +
	    (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    (psRGXKickRDMIN->ui32CmdSize * sizeof(IMG_BYTE)) + 0;

	if (unlikely(psRGXKickRDMIN->ui32ClientUpdateCount > PVRSRV_MAX_SYNCS))
	{
		psRGXKickRDMOUT->eError = PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickRDM_exit;
	}

	if (unlikely(psRGXKickRDMIN->ui32CmdSize > RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXKickRDMOUT->eError = PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickRDM_exit;
	}

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXKickRDMIN), sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE - ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer = (IMG_BYTE *) (void *)psRGXKickRDMIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXKickRDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXKickRDM_exit;
			}
		}
	}

	if (psRGXKickRDMIN->ui32ClientUpdateCount != 0)
	{
		psClientUpdateUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) IMG_OFFSET_ADDR(pArrayArgsBuffer, ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClientUpdateUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE *) IMG_OFFSET_ADDR(pArrayArgsBuffer, ui32NextOffset);
		ui32NextOffset += psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClientUpdateUFOSyncPrimBlockInt2,
		     (const void __user *)psRGXKickRDMIN->phClientUpdateUFOSyncPrimBlock,
		     psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickRDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickRDM_exit;
		}
	}
	if (psRGXKickRDMIN->ui32ClientUpdateCount != 0)
	{
		ui32ClientUpdateOffsetInt =
		    (IMG_UINT32 *) IMG_OFFSET_ADDR(pArrayArgsBuffer, ui32NextOffset);
		ui32NextOffset += psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientUpdateOffsetInt,
		     (const void __user *)psRGXKickRDMIN->pui32ClientUpdateOffset,
		     psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickRDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickRDM_exit;
		}
	}
	if (psRGXKickRDMIN->ui32ClientUpdateCount != 0)
	{
		ui32ClientUpdateValueInt =
		    (IMG_UINT32 *) IMG_OFFSET_ADDR(pArrayArgsBuffer, ui32NextOffset);
		ui32NextOffset += psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientUpdateValueInt,
		     (const void __user *)psRGXKickRDMIN->pui32ClientUpdateValue,
		     psRGXKickRDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickRDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickRDM_exit;
		}
	}

	{
		uiUpdateFenceNameInt =
		    (IMG_CHAR *) IMG_OFFSET_ADDR(pArrayArgsBuffer, ui32NextOffset);
		ui32NextOffset += PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR);
	}

	/* Copy the data over */
	if (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR) > 0)
	{
		if (OSCopyFromUser
		    (NULL, uiUpdateFenceNameInt,
		     (const void __user *)psRGXKickRDMIN->puiUpdateFenceName,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXKickRDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickRDM_exit;
		}
		((IMG_CHAR *) uiUpdateFenceNameInt)[(PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) -
						    1] = '\0';
	}
	if (psRGXKickRDMIN->ui32CmdSize != 0)
	{
		ui8DMCmdInt = (IMG_BYTE *) IMG_OFFSET_ADDR(pArrayArgsBuffer, ui32NextOffset);
		ui32NextOffset += psRGXKickRDMIN->ui32CmdSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXKickRDMIN->ui32CmdSize * sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui8DMCmdInt, (const void __user *)psRGXKickRDMIN->pui8DMCmd,
		     psRGXKickRDMIN->ui32CmdSize * sizeof(IMG_BYTE)) != PVRSRV_OK)
		{
			psRGXKickRDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickRDM_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXKickRDMOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psRayContextInt,
				       hRayContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_RAY_CONTEXT, IMG_TRUE);
	if (unlikely(psRGXKickRDMOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXKickRDM_exit;
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickRDMIN->ui32ClientUpdateCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickRDMOUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
						       (void **)
						       &psClientUpdateUFOSyncPrimBlockInt[i],
						       hClientUpdateUFOSyncPrimBlockInt2[i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickRDMOUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickRDM_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXKickRDMOUT->eError =
	    PVRSRVRGXKickRDMKM(psRayContextInt,
			       psRGXKickRDMIN->ui32ClientCacheOpSeqNum,
			       psRGXKickRDMIN->ui32ClientUpdateCount,
			       psClientUpdateUFOSyncPrimBlockInt,
			       ui32ClientUpdateOffsetInt,
			       ui32ClientUpdateValueInt,
			       psRGXKickRDMIN->hCheckFenceFd,
			       psRGXKickRDMIN->hUpdateTimeline,
			       &psRGXKickRDMOUT->hUpdateFence,
			       uiUpdateFenceNameInt,
			       psRGXKickRDMIN->ui32CmdSize,
			       ui8DMCmdInt,
			       psRGXKickRDMIN->ui32PDumpFlags, psRGXKickRDMIN->ui32ExtJobRef);

RGXKickRDM_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psRayContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hRayContext, PVRSRV_HANDLE_TYPE_RGX_SERVER_RAY_CONTEXT);
	}

	if (hClientUpdateUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickRDMIN->ui32ClientUpdateCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (hClientUpdateUFOSyncPrimBlockInt2[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
							    hClientUpdateUFOSyncPrimBlockInt2[i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
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

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR InitRGXRAYBridge(void);
PVRSRV_ERROR DeinitRGXRAYBridge(void);

/*
 * Register all RGXRAY functions with services
 */
PVRSRV_ERROR InitRGXRAYBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXRAY, PVRSRV_BRIDGE_RGXRAY_RGXCREATERAYCONTEXT,
			      PVRSRVBridgeRGXCreateRayContext, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXRAY, PVRSRV_BRIDGE_RGXRAY_RGXDESTROYRAYCONTEXT,
			      PVRSRVBridgeRGXDestroyRayContext, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXRAY, PVRSRV_BRIDGE_RGXRAY_RGXKICKRDM,
			      PVRSRVBridgeRGXKickRDM, NULL);

	return PVRSRV_OK;
}

/*
 * Unregister all rgxray functions with services
 */
PVRSRV_ERROR DeinitRGXRAYBridge(void)
{

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXRAY, PVRSRV_BRIDGE_RGXRAY_RGXCREATERAYCONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXRAY, PVRSRV_BRIDGE_RGXRAY_RGXDESTROYRAYCONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXRAY, PVRSRV_BRIDGE_RGXRAY_RGXKICKRDM);

	return PVRSRV_OK;
}
