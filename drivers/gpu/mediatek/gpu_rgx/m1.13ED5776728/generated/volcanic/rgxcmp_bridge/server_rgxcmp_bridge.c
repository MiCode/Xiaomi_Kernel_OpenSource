/*******************************************************************************
@File
@Title          Server bridge for rgxcmp
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxcmp
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

#include "rgxcompute.h"

#include "common_rgxcmp_bridge.h"

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

static PVRSRV_ERROR _RGXCreateComputeContextpsComputeContextIntRelease(void
								       *pvData)
{
	PVRSRV_ERROR eError;
	eError =
	    PVRSRVRGXDestroyComputeContextKM((RGX_SERVER_COMPUTE_CONTEXT *)
					     pvData);
	return eError;
}

static_assert(RGXFWIF_STATIC_COMPUTECONTEXT_SIZE <= IMG_UINT32_MAX,
	      "RGXFWIF_STATIC_COMPUTECONTEXT_SIZE must not be larger than IMG_UINT32_MAX");

static IMG_INT
PVRSRVBridgeRGXCreateComputeContext(IMG_UINT32 ui32DispatchTableEntry,
				    IMG_UINT8 * psRGXCreateComputeContextIN_UI8,
				    IMG_UINT8 *
				    psRGXCreateComputeContextOUT_UI8,
				    CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT *psRGXCreateComputeContextIN =
	    (PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT *)
	    IMG_OFFSET_ADDR(psRGXCreateComputeContextIN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT *psRGXCreateComputeContextOUT
	    =
	    (PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT *)
	    IMG_OFFSET_ADDR(psRGXCreateComputeContextOUT_UI8, 0);

	IMG_HANDLE hPrivData = psRGXCreateComputeContextIN->hPrivData;
	IMG_HANDLE hPrivDataInt = NULL;
	IMG_BYTE *psStaticComputeContextStateInt = NULL;
	RGX_SERVER_COMPUTE_CONTEXT *psComputeContextInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize = 0;
	IMG_UINT64 ui64BufferSize =
	    ((IMG_UINT64) psRGXCreateComputeContextIN->
	     ui32StaticComputeContextStateSize * sizeof(IMG_BYTE)) + 0;

	if (unlikely
	    (psRGXCreateComputeContextIN->ui32StaticComputeContextStateSize >
	     RGXFWIF_STATIC_COMPUTECONTEXT_SIZE))
	{
		psRGXCreateComputeContextOUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXCreateComputeContext_exit;
	}

	if (ui64BufferSize > IMG_UINT32_MAX)
	{
		psRGXCreateComputeContextOUT->eError =
		    PVRSRV_ERROR_BRIDGE_BUFFER_TOO_SMALL;
		goto RGXCreateComputeContext_exit;
	}

	ui32BufferSize = (IMG_UINT32) ui64BufferSize;

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXCreateComputeContextIN),
			      sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) (void *)psRGXCreateComputeContextIN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXCreateComputeContextOUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXCreateComputeContext_exit;
			}
		}
	}

	if (psRGXCreateComputeContextIN->ui32StaticComputeContextStateSize != 0)
	{
		psStaticComputeContextStateInt =
		    (IMG_BYTE *) IMG_OFFSET_ADDR(pArrayArgsBuffer,
						 ui32NextOffset);
		ui32NextOffset +=
		    psRGXCreateComputeContextIN->
		    ui32StaticComputeContextStateSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXCreateComputeContextIN->ui32StaticComputeContextStateSize *
	    sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, psStaticComputeContextStateInt,
		     (const void __user *)psRGXCreateComputeContextIN->
		     psStaticComputeContextState,
		     psRGXCreateComputeContextIN->
		     ui32StaticComputeContextStateSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXCreateComputeContextOUT->eError =
			    PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXCreateComputeContext_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXCreateComputeContextOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&hPrivDataInt,
				       hPrivData,
				       PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA,
				       IMG_TRUE);
	if (unlikely(psRGXCreateComputeContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateComputeContext_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXCreateComputeContextOUT->eError =
	    PVRSRVRGXCreateComputeContextKM(psConnection,
					    OSGetDevNode(psConnection),
					    psRGXCreateComputeContextIN->
					    ui32Priority, hPrivDataInt,
					    psRGXCreateComputeContextIN->
					    ui32StaticComputeContextStateSize,
					    psStaticComputeContextStateInt,
					    psRGXCreateComputeContextIN->
					    ui32PackedCCBSizeU88,
					    psRGXCreateComputeContextIN->
					    ui32ContextFlags,
					    psRGXCreateComputeContextIN->
					    ui64RobustnessAddress,
					    psRGXCreateComputeContextIN->
					    ui32MaxDeadlineMS,
					    &psComputeContextInt);
	/* Exit early if bridged call fails */
	if (unlikely(psRGXCreateComputeContextOUT->eError != PVRSRV_OK))
	{
		goto RGXCreateComputeContext_exit;
	}

	/* Lock over handle creation. */
	LockHandle(psConnection->psHandleBase);

	psRGXCreateComputeContextOUT->eError =
	    PVRSRVAllocHandleUnlocked(psConnection->psHandleBase,
				      &psRGXCreateComputeContextOUT->
				      hComputeContext,
				      (void *)psComputeContextInt,
				      PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
				      PVRSRV_HANDLE_ALLOC_FLAG_MULTI,
				      (PFN_HANDLE_RELEASE) &
				      _RGXCreateComputeContextpsComputeContextIntRelease);
	if (unlikely(psRGXCreateComputeContextOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXCreateComputeContext_exit;
	}

	/* Release now we have created handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXCreateComputeContext_exit:

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

	if (psRGXCreateComputeContextOUT->eError != PVRSRV_OK)
	{
		if (psComputeContextInt)
		{
			PVRSRVRGXDestroyComputeContextKM(psComputeContextInt);
		}
	}

	/* Allocated space should be equal to the last updated offset */
#ifdef PVRSRV_NEED_PVR_ASSERT
	if (psRGXCreateComputeContextOUT->eError == PVRSRV_OK)
		PVR_ASSERT(ui32BufferSize == ui32NextOffset);
#endif /* PVRSRV_NEED_PVR_ASSERT */

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyComputeContext(IMG_UINT32 ui32DispatchTableEntry,
				     IMG_UINT8 *
				     psRGXDestroyComputeContextIN_UI8,
				     IMG_UINT8 *
				     psRGXDestroyComputeContextOUT_UI8,
				     CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT *psRGXDestroyComputeContextIN
	    =
	    (PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT *)
	    IMG_OFFSET_ADDR(psRGXDestroyComputeContextIN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXDESTROYCOMPUTECONTEXT
	    *psRGXDestroyComputeContextOUT =
	    (PVRSRV_BRIDGE_OUT_RGXDESTROYCOMPUTECONTEXT *)
	    IMG_OFFSET_ADDR(psRGXDestroyComputeContextOUT_UI8, 0);

	/* Lock over handle destruction. */
	LockHandle(psConnection->psHandleBase);

	psRGXDestroyComputeContextOUT->eError =
	    PVRSRVDestroyHandleStagedUnlocked(psConnection->psHandleBase,
					      (IMG_HANDLE)
					      psRGXDestroyComputeContextIN->
					      hComputeContext,
					      PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
	if (unlikely
	    ((psRGXDestroyComputeContextOUT->eError != PVRSRV_OK)
	     && (psRGXDestroyComputeContextOUT->eError != PVRSRV_ERROR_RETRY)))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: %s",
			 __func__,
			 PVRSRVGetErrorString(psRGXDestroyComputeContextOUT->
					      eError)));
		UnlockHandle(psConnection->psHandleBase);
		goto RGXDestroyComputeContext_exit;
	}

	/* Release now we have destroyed handles. */
	UnlockHandle(psConnection->psHandleBase);

RGXDestroyComputeContext_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXFlushComputeData(IMG_UINT32 ui32DispatchTableEntry,
				IMG_UINT8 * psRGXFlushComputeDataIN_UI8,
				IMG_UINT8 * psRGXFlushComputeDataOUT_UI8,
				CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA *psRGXFlushComputeDataIN =
	    (PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA *)
	    IMG_OFFSET_ADDR(psRGXFlushComputeDataIN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXFLUSHCOMPUTEDATA *psRGXFlushComputeDataOUT =
	    (PVRSRV_BRIDGE_OUT_RGXFLUSHCOMPUTEDATA *)
	    IMG_OFFSET_ADDR(psRGXFlushComputeDataOUT_UI8, 0);

	IMG_HANDLE hComputeContext = psRGXFlushComputeDataIN->hComputeContext;
	RGX_SERVER_COMPUTE_CONTEXT *psComputeContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXFlushComputeDataOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psComputeContextInt,
				       hComputeContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXFlushComputeDataOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXFlushComputeData_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXFlushComputeDataOUT->eError =
	    PVRSRVRGXFlushComputeDataKM(psComputeContextInt);

RGXFlushComputeData_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psComputeContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hComputeContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXSetComputeContextPriority(IMG_UINT32 ui32DispatchTableEntry,
					 IMG_UINT8 *
					 psRGXSetComputeContextPriorityIN_UI8,
					 IMG_UINT8 *
					 psRGXSetComputeContextPriorityOUT_UI8,
					 CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY
	    *psRGXSetComputeContextPriorityIN =
	    (PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY *)
	    IMG_OFFSET_ADDR(psRGXSetComputeContextPriorityIN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXSETCOMPUTECONTEXTPRIORITY
	    *psRGXSetComputeContextPriorityOUT =
	    (PVRSRV_BRIDGE_OUT_RGXSETCOMPUTECONTEXTPRIORITY *)
	    IMG_OFFSET_ADDR(psRGXSetComputeContextPriorityOUT_UI8, 0);

	IMG_HANDLE hComputeContext =
	    psRGXSetComputeContextPriorityIN->hComputeContext;
	RGX_SERVER_COMPUTE_CONTEXT *psComputeContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXSetComputeContextPriorityOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psComputeContextInt,
				       hComputeContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXSetComputeContextPriorityOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXSetComputeContextPriority_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXSetComputeContextPriorityOUT->eError =
	    PVRSRVRGXSetComputeContextPriorityKM(psConnection,
						 OSGetDevNode(psConnection),
						 psComputeContextInt,
						 psRGXSetComputeContextPriorityIN->
						 ui32Priority);

RGXSetComputeContextPriority_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psComputeContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hComputeContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXGetLastComputeContextResetReason(IMG_UINT32
						ui32DispatchTableEntry,
						IMG_UINT8 *
						psRGXGetLastComputeContextResetReasonIN_UI8,
						IMG_UINT8 *
						psRGXGetLastComputeContextResetReasonOUT_UI8,
						CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXGETLASTCOMPUTECONTEXTRESETREASON
	    *psRGXGetLastComputeContextResetReasonIN =
	    (PVRSRV_BRIDGE_IN_RGXGETLASTCOMPUTECONTEXTRESETREASON *)
	    IMG_OFFSET_ADDR(psRGXGetLastComputeContextResetReasonIN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXGETLASTCOMPUTECONTEXTRESETREASON
	    *psRGXGetLastComputeContextResetReasonOUT =
	    (PVRSRV_BRIDGE_OUT_RGXGETLASTCOMPUTECONTEXTRESETREASON *)
	    IMG_OFFSET_ADDR(psRGXGetLastComputeContextResetReasonOUT_UI8, 0);

	IMG_HANDLE hComputeContext =
	    psRGXGetLastComputeContextResetReasonIN->hComputeContext;
	RGX_SERVER_COMPUTE_CONTEXT *psComputeContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXGetLastComputeContextResetReasonOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psComputeContextInt,
				       hComputeContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
				       IMG_TRUE);
	if (unlikely
	    (psRGXGetLastComputeContextResetReasonOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXGetLastComputeContextResetReason_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXGetLastComputeContextResetReasonOUT->eError =
	    PVRSRVRGXGetLastComputeContextResetReasonKM(psComputeContextInt,
							&psRGXGetLastComputeContextResetReasonOUT->
							ui32LastResetReason,
							&psRGXGetLastComputeContextResetReasonOUT->
							ui32LastResetJobRef);

RGXGetLastComputeContextResetReason_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psComputeContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hComputeContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXNotifyComputeWriteOffsetUpdate(IMG_UINT32 ui32DispatchTableEntry,
					      IMG_UINT8 *
					      psRGXNotifyComputeWriteOffsetUpdateIN_UI8,
					      IMG_UINT8 *
					      psRGXNotifyComputeWriteOffsetUpdateOUT_UI8,
					      CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXNOTIFYCOMPUTEWRITEOFFSETUPDATE
	    *psRGXNotifyComputeWriteOffsetUpdateIN =
	    (PVRSRV_BRIDGE_IN_RGXNOTIFYCOMPUTEWRITEOFFSETUPDATE *)
	    IMG_OFFSET_ADDR(psRGXNotifyComputeWriteOffsetUpdateIN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXNOTIFYCOMPUTEWRITEOFFSETUPDATE
	    *psRGXNotifyComputeWriteOffsetUpdateOUT =
	    (PVRSRV_BRIDGE_OUT_RGXNOTIFYCOMPUTEWRITEOFFSETUPDATE *)
	    IMG_OFFSET_ADDR(psRGXNotifyComputeWriteOffsetUpdateOUT_UI8, 0);

	IMG_HANDLE hComputeContext =
	    psRGXNotifyComputeWriteOffsetUpdateIN->hComputeContext;
	RGX_SERVER_COMPUTE_CONTEXT *psComputeContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXNotifyComputeWriteOffsetUpdateOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psComputeContextInt,
				       hComputeContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
				       IMG_TRUE);
	if (unlikely
	    (psRGXNotifyComputeWriteOffsetUpdateOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXNotifyComputeWriteOffsetUpdate_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXNotifyComputeWriteOffsetUpdateOUT->eError =
	    PVRSRVRGXNotifyComputeWriteOffsetUpdateKM(psComputeContextInt);

RGXNotifyComputeWriteOffsetUpdate_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psComputeContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hComputeContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

static_assert(PVRSRV_MAX_SYNCS <= IMG_UINT32_MAX,
	      "PVRSRV_MAX_SYNCS must not be larger than IMG_UINT32_MAX");
static_assert(PVRSRV_SYNC_NAME_LENGTH <= IMG_UINT32_MAX,
	      "PVRSRV_SYNC_NAME_LENGTH must not be larger than IMG_UINT32_MAX");
static_assert(RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE <= IMG_UINT32_MAX,
	      "RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE must not be larger than IMG_UINT32_MAX");

static IMG_INT
PVRSRVBridgeRGXKickCDM2(IMG_UINT32 ui32DispatchTableEntry,
			IMG_UINT8 * psRGXKickCDM2IN_UI8,
			IMG_UINT8 * psRGXKickCDM2OUT_UI8,
			CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXKICKCDM2 *psRGXKickCDM2IN =
	    (PVRSRV_BRIDGE_IN_RGXKICKCDM2 *)
	    IMG_OFFSET_ADDR(psRGXKickCDM2IN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXKICKCDM2 *psRGXKickCDM2OUT =
	    (PVRSRV_BRIDGE_OUT_RGXKICKCDM2 *)
	    IMG_OFFSET_ADDR(psRGXKickCDM2OUT_UI8, 0);

	IMG_HANDLE hComputeContext = psRGXKickCDM2IN->hComputeContext;
	RGX_SERVER_COMPUTE_CONTEXT *psComputeContextInt = NULL;
	SYNC_PRIMITIVE_BLOCK **psClientUpdateUFOSyncPrimBlockInt = NULL;
	IMG_HANDLE *hClientUpdateUFOSyncPrimBlockInt2 = NULL;
	IMG_UINT32 *ui32ClientUpdateOffsetInt = NULL;
	IMG_UINT32 *ui32ClientUpdateValueInt = NULL;
	IMG_CHAR *uiUpdateFenceNameInt = NULL;
	IMG_BYTE *psDMCmdInt = NULL;

	IMG_UINT32 ui32NextOffset = 0;
	IMG_BYTE *pArrayArgsBuffer = NULL;
#if !defined(INTEGRITY_OS)
	IMG_BOOL bHaveEnoughSpace = IMG_FALSE;
#endif

	IMG_UINT32 ui32BufferSize = 0;
	IMG_UINT64 ui64BufferSize =
	    ((IMG_UINT64) psRGXKickCDM2IN->ui32ClientUpdateCount *
	     sizeof(SYNC_PRIMITIVE_BLOCK *)) +
	    ((IMG_UINT64) psRGXKickCDM2IN->ui32ClientUpdateCount *
	     sizeof(IMG_HANDLE)) +
	    ((IMG_UINT64) psRGXKickCDM2IN->ui32ClientUpdateCount *
	     sizeof(IMG_UINT32)) +
	    ((IMG_UINT64) psRGXKickCDM2IN->ui32ClientUpdateCount *
	     sizeof(IMG_UINT32)) +
	    ((IMG_UINT64) PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) +
	    ((IMG_UINT64) psRGXKickCDM2IN->ui32CmdSize * sizeof(IMG_BYTE)) + 0;

	if (unlikely(psRGXKickCDM2IN->ui32ClientUpdateCount > PVRSRV_MAX_SYNCS))
	{
		psRGXKickCDM2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickCDM2_exit;
	}

	if (unlikely
	    (psRGXKickCDM2IN->ui32CmdSize >
	     RGXFWIF_DM_INDEPENDENT_KICK_CMD_SIZE))
	{
		psRGXKickCDM2OUT->eError =
		    PVRSRV_ERROR_BRIDGE_ARRAY_SIZE_TOO_BIG;
		goto RGXKickCDM2_exit;
	}

	if (ui64BufferSize > IMG_UINT32_MAX)
	{
		psRGXKickCDM2OUT->eError = PVRSRV_ERROR_BRIDGE_BUFFER_TOO_SMALL;
		goto RGXKickCDM2_exit;
	}

	ui32BufferSize = (IMG_UINT32) ui64BufferSize;

	if (ui32BufferSize != 0)
	{
#if !defined(INTEGRITY_OS)
		/* Try to use remainder of input buffer for copies if possible, word-aligned for safety. */
		IMG_UINT32 ui32InBufferOffset =
		    PVR_ALIGN(sizeof(*psRGXKickCDM2IN), sizeof(unsigned long));
		IMG_UINT32 ui32InBufferExcessSize =
		    ui32InBufferOffset >=
		    PVRSRV_MAX_BRIDGE_IN_SIZE ? 0 : PVRSRV_MAX_BRIDGE_IN_SIZE -
		    ui32InBufferOffset;

		bHaveEnoughSpace = ui32BufferSize <= ui32InBufferExcessSize;
		if (bHaveEnoughSpace)
		{
			IMG_BYTE *pInputBuffer =
			    (IMG_BYTE *) (void *)psRGXKickCDM2IN;

			pArrayArgsBuffer = &pInputBuffer[ui32InBufferOffset];
		}
		else
#endif
		{
			pArrayArgsBuffer = OSAllocMemNoStats(ui32BufferSize);

			if (!pArrayArgsBuffer)
			{
				psRGXKickCDM2OUT->eError =
				    PVRSRV_ERROR_OUT_OF_MEMORY;
				goto RGXKickCDM2_exit;
			}
		}
	}

	if (psRGXKickCDM2IN->ui32ClientUpdateCount != 0)
	{
		psClientUpdateUFOSyncPrimBlockInt =
		    (SYNC_PRIMITIVE_BLOCK **) IMG_OFFSET_ADDR(pArrayArgsBuffer,
							      ui32NextOffset);
		OSCachedMemSet(psClientUpdateUFOSyncPrimBlockInt, 0,
			       psRGXKickCDM2IN->ui32ClientUpdateCount *
			       sizeof(SYNC_PRIMITIVE_BLOCK *));
		ui32NextOffset +=
		    psRGXKickCDM2IN->ui32ClientUpdateCount *
		    sizeof(SYNC_PRIMITIVE_BLOCK *);
		hClientUpdateUFOSyncPrimBlockInt2 =
		    (IMG_HANDLE *) IMG_OFFSET_ADDR(pArrayArgsBuffer,
						   ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickCDM2IN->ui32ClientUpdateCount * sizeof(IMG_HANDLE);
	}

	/* Copy the data over */
	if (psRGXKickCDM2IN->ui32ClientUpdateCount * sizeof(IMG_HANDLE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, hClientUpdateUFOSyncPrimBlockInt2,
		     (const void __user *)psRGXKickCDM2IN->
		     phClientUpdateUFOSyncPrimBlock,
		     psRGXKickCDM2IN->ui32ClientUpdateCount *
		     sizeof(IMG_HANDLE)) != PVRSRV_OK)
		{
			psRGXKickCDM2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickCDM2_exit;
		}
	}
	if (psRGXKickCDM2IN->ui32ClientUpdateCount != 0)
	{
		ui32ClientUpdateOffsetInt =
		    (IMG_UINT32 *) IMG_OFFSET_ADDR(pArrayArgsBuffer,
						   ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickCDM2IN->ui32ClientUpdateCount * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickCDM2IN->ui32ClientUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientUpdateOffsetInt,
		     (const void __user *)psRGXKickCDM2IN->
		     pui32ClientUpdateOffset,
		     psRGXKickCDM2IN->ui32ClientUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickCDM2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickCDM2_exit;
		}
	}
	if (psRGXKickCDM2IN->ui32ClientUpdateCount != 0)
	{
		ui32ClientUpdateValueInt =
		    (IMG_UINT32 *) IMG_OFFSET_ADDR(pArrayArgsBuffer,
						   ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickCDM2IN->ui32ClientUpdateCount * sizeof(IMG_UINT32);
	}

	/* Copy the data over */
	if (psRGXKickCDM2IN->ui32ClientUpdateCount * sizeof(IMG_UINT32) > 0)
	{
		if (OSCopyFromUser
		    (NULL, ui32ClientUpdateValueInt,
		     (const void __user *)psRGXKickCDM2IN->
		     pui32ClientUpdateValue,
		     psRGXKickCDM2IN->ui32ClientUpdateCount *
		     sizeof(IMG_UINT32)) != PVRSRV_OK)
		{
			psRGXKickCDM2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickCDM2_exit;
		}
	}

	{
		uiUpdateFenceNameInt =
		    (IMG_CHAR *) IMG_OFFSET_ADDR(pArrayArgsBuffer,
						 ui32NextOffset);
		ui32NextOffset += PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR);
	}

	/* Copy the data over */
	if (PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR) > 0)
	{
		if (OSCopyFromUser
		    (NULL, uiUpdateFenceNameInt,
		     (const void __user *)psRGXKickCDM2IN->puiUpdateFenceName,
		     PVRSRV_SYNC_NAME_LENGTH * sizeof(IMG_CHAR)) != PVRSRV_OK)
		{
			psRGXKickCDM2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickCDM2_exit;
		}
		((IMG_CHAR *)
		 uiUpdateFenceNameInt)[(PVRSRV_SYNC_NAME_LENGTH *
					sizeof(IMG_CHAR)) - 1] = '\0';
	}
	if (psRGXKickCDM2IN->ui32CmdSize != 0)
	{
		psDMCmdInt =
		    (IMG_BYTE *) IMG_OFFSET_ADDR(pArrayArgsBuffer,
						 ui32NextOffset);
		ui32NextOffset +=
		    psRGXKickCDM2IN->ui32CmdSize * sizeof(IMG_BYTE);
	}

	/* Copy the data over */
	if (psRGXKickCDM2IN->ui32CmdSize * sizeof(IMG_BYTE) > 0)
	{
		if (OSCopyFromUser
		    (NULL, psDMCmdInt,
		     (const void __user *)psRGXKickCDM2IN->psDMCmd,
		     psRGXKickCDM2IN->ui32CmdSize * sizeof(IMG_BYTE)) !=
		    PVRSRV_OK)
		{
			psRGXKickCDM2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

			goto RGXKickCDM2_exit;
		}
	}

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXKickCDM2OUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psComputeContextInt,
				       hComputeContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXKickCDM2OUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXKickCDM2_exit;
	}

	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickCDM2IN->ui32ClientUpdateCount; i++)
		{
			/* Look up the address from the handle */
			psRGXKickCDM2OUT->eError =
			    PVRSRVLookupHandleUnlocked(psConnection->
						       psHandleBase,
						       (void **)
						       &psClientUpdateUFOSyncPrimBlockInt
						       [i],
						       hClientUpdateUFOSyncPrimBlockInt2
						       [i],
						       PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK,
						       IMG_TRUE);
			if (unlikely(psRGXKickCDM2OUT->eError != PVRSRV_OK))
			{
				UnlockHandle(psConnection->psHandleBase);
				goto RGXKickCDM2_exit;
			}
		}
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXKickCDM2OUT->eError =
	    PVRSRVRGXKickCDMKM(psComputeContextInt,
			       psRGXKickCDM2IN->ui32ClientCacheOpSeqNum,
			       psRGXKickCDM2IN->ui32ClientUpdateCount,
			       psClientUpdateUFOSyncPrimBlockInt,
			       ui32ClientUpdateOffsetInt,
			       ui32ClientUpdateValueInt,
			       psRGXKickCDM2IN->hCheckFenceFd,
			       psRGXKickCDM2IN->hUpdateTimeline,
			       &psRGXKickCDM2OUT->hUpdateFence,
			       uiUpdateFenceNameInt,
			       psRGXKickCDM2IN->ui32CmdSize,
			       psDMCmdInt,
			       psRGXKickCDM2IN->ui32PDumpFlags,
			       psRGXKickCDM2IN->ui32ExtJobRef,
			       psRGXKickCDM2IN->ui32NumOfWorkgroups,
			       psRGXKickCDM2IN->ui32NumOfWorkitems,
			       psRGXKickCDM2IN->ui64DeadlineInus);

RGXKickCDM2_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psComputeContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hComputeContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
	}

	if (hClientUpdateUFOSyncPrimBlockInt2)
	{
		IMG_UINT32 i;

		for (i = 0; i < psRGXKickCDM2IN->ui32ClientUpdateCount; i++)
		{

			/* Unreference the previously looked up handle */
			if (psClientUpdateUFOSyncPrimBlockInt[i])
			{
				PVRSRVReleaseHandleUnlocked(psConnection->
							    psHandleBase,
							    hClientUpdateUFOSyncPrimBlockInt2
							    [i],
							    PVRSRV_HANDLE_TYPE_SYNC_PRIMITIVE_BLOCK);
			}
		}
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	/* Allocated space should be equal to the last updated offset */
#ifdef PVRSRV_NEED_PVR_ASSERT
	if (psRGXKickCDM2OUT->eError == PVRSRV_OK)
		PVR_ASSERT(ui32BufferSize == ui32NextOffset);
#endif /* PVRSRV_NEED_PVR_ASSERT */

#if defined(INTEGRITY_OS)
	if (pArrayArgsBuffer)
#else
	if (!bHaveEnoughSpace && pArrayArgsBuffer)
#endif
		OSFreeMemNoStats(pArrayArgsBuffer);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXSetComputeContextProperty(IMG_UINT32 ui32DispatchTableEntry,
					 IMG_UINT8 *
					 psRGXSetComputeContextPropertyIN_UI8,
					 IMG_UINT8 *
					 psRGXSetComputeContextPropertyOUT_UI8,
					 CONNECTION_DATA * psConnection)
{
	PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPROPERTY
	    *psRGXSetComputeContextPropertyIN =
	    (PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPROPERTY *)
	    IMG_OFFSET_ADDR(psRGXSetComputeContextPropertyIN_UI8, 0);
	PVRSRV_BRIDGE_OUT_RGXSETCOMPUTECONTEXTPROPERTY
	    *psRGXSetComputeContextPropertyOUT =
	    (PVRSRV_BRIDGE_OUT_RGXSETCOMPUTECONTEXTPROPERTY *)
	    IMG_OFFSET_ADDR(psRGXSetComputeContextPropertyOUT_UI8, 0);

	IMG_HANDLE hComputeContext =
	    psRGXSetComputeContextPropertyIN->hComputeContext;
	RGX_SERVER_COMPUTE_CONTEXT *psComputeContextInt = NULL;

	/* Lock over handle lookup. */
	LockHandle(psConnection->psHandleBase);

	/* Look up the address from the handle */
	psRGXSetComputeContextPropertyOUT->eError =
	    PVRSRVLookupHandleUnlocked(psConnection->psHandleBase,
				       (void **)&psComputeContextInt,
				       hComputeContext,
				       PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
				       IMG_TRUE);
	if (unlikely(psRGXSetComputeContextPropertyOUT->eError != PVRSRV_OK))
	{
		UnlockHandle(psConnection->psHandleBase);
		goto RGXSetComputeContextProperty_exit;
	}
	/* Release now we have looked up handles. */
	UnlockHandle(psConnection->psHandleBase);

	psRGXSetComputeContextPropertyOUT->eError =
	    PVRSRVRGXSetComputeContextPropertyKM(psComputeContextInt,
						 psRGXSetComputeContextPropertyIN->
						 ui32Property,
						 psRGXSetComputeContextPropertyIN->
						 ui64Input,
						 &psRGXSetComputeContextPropertyOUT->
						 ui64Output);

RGXSetComputeContextProperty_exit:

	/* Lock over handle lookup cleanup. */
	LockHandle(psConnection->psHandleBase);

	/* Unreference the previously looked up handle */
	if (psComputeContextInt)
	{
		PVRSRVReleaseHandleUnlocked(psConnection->psHandleBase,
					    hComputeContext,
					    PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
	}
	/* Release now we have cleaned up look up handles. */
	UnlockHandle(psConnection->psHandleBase);

	return 0;
}

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR InitRGXCMPBridge(void);
PVRSRV_ERROR DeinitRGXCMPBridge(void);

/*
 * Register all RGXCMP functions with services
 */
PVRSRV_ERROR InitRGXCMPBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
			      PVRSRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT,
			      PVRSRVBridgeRGXCreateComputeContext, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
			      PVRSRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT,
			      PVRSRVBridgeRGXDestroyComputeContext, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
			      PVRSRV_BRIDGE_RGXCMP_RGXFLUSHCOMPUTEDATA,
			      PVRSRVBridgeRGXFlushComputeData, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
			      PVRSRV_BRIDGE_RGXCMP_RGXSETCOMPUTECONTEXTPRIORITY,
			      PVRSRVBridgeRGXSetComputeContextPriority, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
			      PVRSRV_BRIDGE_RGXCMP_RGXGETLASTCOMPUTECONTEXTRESETREASON,
			      PVRSRVBridgeRGXGetLastComputeContextResetReason,
			      NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
			      PVRSRV_BRIDGE_RGXCMP_RGXNOTIFYCOMPUTEWRITEOFFSETUPDATE,
			      PVRSRVBridgeRGXNotifyComputeWriteOffsetUpdate,
			      NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
			      PVRSRV_BRIDGE_RGXCMP_RGXKICKCDM2,
			      PVRSRVBridgeRGXKickCDM2, NULL);

	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
			      PVRSRV_BRIDGE_RGXCMP_RGXSETCOMPUTECONTEXTPROPERTY,
			      PVRSRVBridgeRGXSetComputeContextProperty, NULL);

	return PVRSRV_OK;
}

/*
 * Unregister all rgxcmp functions with services
 */
PVRSRV_ERROR DeinitRGXCMPBridge(void)
{

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
				PVRSRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
				PVRSRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
				PVRSRV_BRIDGE_RGXCMP_RGXFLUSHCOMPUTEDATA);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
				PVRSRV_BRIDGE_RGXCMP_RGXSETCOMPUTECONTEXTPRIORITY);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
				PVRSRV_BRIDGE_RGXCMP_RGXGETLASTCOMPUTECONTEXTRESETREASON);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
				PVRSRV_BRIDGE_RGXCMP_RGXNOTIFYCOMPUTEWRITEOFFSETUPDATE);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
				PVRSRV_BRIDGE_RGXCMP_RGXKICKCDM2);

	UnsetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP,
				PVRSRV_BRIDGE_RGXCMP_RGXSETCOMPUTECONTEXTPROPERTY);

	return PVRSRV_OK;
}
