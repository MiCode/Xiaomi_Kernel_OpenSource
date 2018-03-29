/*************************************************************************/ /*!
@File
@Title          Server bridge for cacherangebased
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for cacherangebased
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

#include "cache.h"


#include "common_cacherangebased_bridge.h"

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
PVRSRVBridgeCacheOpQueue(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_CACHEOPQUEUE *psCacheOpQueueIN,
					  PVRSRV_BRIDGE_OUT_CACHEOPQUEUE *psCacheOpQueueOUT,
					 CONNECTION_DATA *psConnection)
{
	PMR * *psPMRInt = NULL;
	IMG_HANDLE *hPMRInt2 = NULL;
	IMG_DEVMEM_OFFSET_T *uiOffsetInt = NULL;
	IMG_DEVMEM_SIZE_T *uiSizeInt = NULL;
	PVRSRV_CACHE_OP *iuCacheOpInt = NULL;




	if (psCacheOpQueueIN->ui32NumCacheOps != 0)
	{
		psPMRInt = OSAllocZMemNoStats(psCacheOpQueueIN->ui32NumCacheOps * sizeof(PMR *));
		if (!psPMRInt)
		{
			psCacheOpQueueOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto CacheOpQueue_exit;
		}
		hPMRInt2 = OSAllocMemNoStats(psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_HANDLE));
		if (!hPMRInt2)
		{
			psCacheOpQueueOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto CacheOpQueue_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psCacheOpQueueIN->phPMR, psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hPMRInt2, psCacheOpQueueIN->phPMR,
				psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psCacheOpQueueOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto CacheOpQueue_exit;
			}
	if (psCacheOpQueueIN->ui32NumCacheOps != 0)
	{
		uiOffsetInt = OSAllocZMemNoStats(psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_DEVMEM_OFFSET_T));
		if (!uiOffsetInt)
		{
			psCacheOpQueueOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto CacheOpQueue_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psCacheOpQueueIN->puiOffset, psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_DEVMEM_OFFSET_T))
				|| (OSCopyFromUser(NULL, uiOffsetInt, psCacheOpQueueIN->puiOffset,
				psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_DEVMEM_OFFSET_T)) != PVRSRV_OK) )
			{
				psCacheOpQueueOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto CacheOpQueue_exit;
			}
	if (psCacheOpQueueIN->ui32NumCacheOps != 0)
	{
		uiSizeInt = OSAllocZMemNoStats(psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_DEVMEM_SIZE_T));
		if (!uiSizeInt)
		{
			psCacheOpQueueOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto CacheOpQueue_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psCacheOpQueueIN->puiSize, psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_DEVMEM_SIZE_T))
				|| (OSCopyFromUser(NULL, uiSizeInt, psCacheOpQueueIN->puiSize,
				psCacheOpQueueIN->ui32NumCacheOps * sizeof(IMG_DEVMEM_SIZE_T)) != PVRSRV_OK) )
			{
				psCacheOpQueueOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto CacheOpQueue_exit;
			}
	if (psCacheOpQueueIN->ui32NumCacheOps != 0)
	{
		iuCacheOpInt = OSAllocZMemNoStats(psCacheOpQueueIN->ui32NumCacheOps * sizeof(PVRSRV_CACHE_OP));
		if (!iuCacheOpInt)
		{
			psCacheOpQueueOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto CacheOpQueue_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (void*) psCacheOpQueueIN->piuCacheOp, psCacheOpQueueIN->ui32NumCacheOps * sizeof(PVRSRV_CACHE_OP))
				|| (OSCopyFromUser(NULL, iuCacheOpInt, psCacheOpQueueIN->piuCacheOp,
				psCacheOpQueueIN->ui32NumCacheOps * sizeof(PVRSRV_CACHE_OP)) != PVRSRV_OK) )
			{
				psCacheOpQueueOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto CacheOpQueue_exit;
			}






	{
		IMG_UINT32 i;

		for (i=0;i<psCacheOpQueueIN->ui32NumCacheOps;i++)
		{
				{
					/* Look up the address from the handle */
					psCacheOpQueueOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt[i],
											hPMRInt2[i],
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psCacheOpQueueOUT->eError != PVRSRV_OK)
					{
						goto CacheOpQueue_exit;
					}
				}
		}
	}

	psCacheOpQueueOUT->eError =
		CacheOpQueue(
					psCacheOpQueueIN->ui32NumCacheOps,
					psPMRInt,
					uiOffsetInt,
					uiSizeInt,
					iuCacheOpInt,
					&psCacheOpQueueOUT->ui32CacheOpSeqNum);




CacheOpQueue_exit:






	{
		IMG_UINT32 i;

		for (i=0;i<psCacheOpQueueIN->ui32NumCacheOps;i++)
		{
				{
					/* Unreference the previously looked up handle */
						if(psPMRInt[i])
						{
							PVRSRVReleaseHandle(psConnection->psHandleBase,
											hPMRInt2[i],
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
						}
				}
		}
	}

	if (psPMRInt)
		OSFreeMemNoStats(psPMRInt);
	if (hPMRInt2)
		OSFreeMemNoStats(hPMRInt2);
	if (uiOffsetInt)
		OSFreeMemNoStats(uiOffsetInt);
	if (uiSizeInt)
		OSFreeMemNoStats(uiSizeInt);
	if (iuCacheOpInt)
		OSFreeMemNoStats(iuCacheOpInt);

	return 0;
}


static IMG_INT
PVRSRVBridgeCacheOpExec(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_CACHEOPEXEC *psCacheOpExecIN,
					  PVRSRV_BRIDGE_OUT_CACHEOPEXEC *psCacheOpExecOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMR = psCacheOpExecIN->hPMR;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psCacheOpExecOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psCacheOpExecOUT->eError != PVRSRV_OK)
					{
						goto CacheOpExec_exit;
					}
				}

	psCacheOpExecOUT->eError =
		CacheOpExec(
					psPMRInt,
					psCacheOpExecIN->uiOffset,
					psCacheOpExecIN->uiSize,
					psCacheOpExecIN->iuCacheOp);




CacheOpExec_exit:






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
PVRSRVBridgeCacheOpSetTimeline(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_CACHEOPSETTIMELINE *psCacheOpSetTimelineIN,
					  PVRSRV_BRIDGE_OUT_CACHEOPSETTIMELINE *psCacheOpSetTimelineOUT,
					 CONNECTION_DATA *psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psConnection);





	psCacheOpSetTimelineOUT->eError =
		CacheOpSetTimeline(
					psCacheOpSetTimelineIN->i32OpTimeline);







	return 0;
}


static IMG_INT
PVRSRVBridgeCacheOpLog(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_CACHEOPLOG *psCacheOpLogIN,
					  PVRSRV_BRIDGE_OUT_CACHEOPLOG *psCacheOpLogOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPMR = psCacheOpLogIN->hPMR;
	PMR * psPMRInt = NULL;










				{
					/* Look up the address from the handle */
					psCacheOpLogOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(void **) &psPMRInt,
											hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR,
											IMG_TRUE);
					if(psCacheOpLogOUT->eError != PVRSRV_OK)
					{
						goto CacheOpLog_exit;
					}
				}

	psCacheOpLogOUT->eError =
		CacheOpLog(
					psPMRInt,
					psCacheOpLogIN->uiOffset,
					psCacheOpLogIN->uiSize,
					psCacheOpLogIN->i64QueuedTimeUs,
					psCacheOpLogIN->i64ExecuteTimeUs,
					psCacheOpLogIN->iuCacheOp);




CacheOpLog_exit:






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
PVRSRVBridgeCacheOpGetLineSize(IMG_UINT32 ui32DispatchTableEntry,
					  PVRSRV_BRIDGE_IN_CACHEOPGETLINESIZE *psCacheOpGetLineSizeIN,
					  PVRSRV_BRIDGE_OUT_CACHEOPGETLINESIZE *psCacheOpGetLineSizeOUT,
					 CONNECTION_DATA *psConnection)
{

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psCacheOpGetLineSizeIN);





	psCacheOpGetLineSizeOUT->eError =
		CacheOpGetLineSize(
					&psCacheOpGetLineSizeOUT->ui32L1DataCacheLineSize);







	return 0;
}




/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */

static IMG_BOOL bUseLock = IMG_TRUE;

PVRSRV_ERROR InitCACHERANGEBASEDBridge(void);
PVRSRV_ERROR DeinitCACHERANGEBASEDBridge(void);

/*
 * Register all CACHERANGEBASED functions with services
 */
PVRSRV_ERROR InitCACHERANGEBASEDBridge(void)
{

	SetDispatchTableEntry(PVRSRV_BRIDGE_CACHERANGEBASED, PVRSRV_BRIDGE_CACHERANGEBASED_CACHEOPQUEUE, PVRSRVBridgeCacheOpQueue,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_CACHERANGEBASED, PVRSRV_BRIDGE_CACHERANGEBASED_CACHEOPEXEC, PVRSRVBridgeCacheOpExec,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_CACHERANGEBASED, PVRSRV_BRIDGE_CACHERANGEBASED_CACHEOPSETTIMELINE, PVRSRVBridgeCacheOpSetTimeline,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_CACHERANGEBASED, PVRSRV_BRIDGE_CACHERANGEBASED_CACHEOPLOG, PVRSRVBridgeCacheOpLog,
					NULL, bUseLock);

	SetDispatchTableEntry(PVRSRV_BRIDGE_CACHERANGEBASED, PVRSRV_BRIDGE_CACHERANGEBASED_CACHEOPGETLINESIZE, PVRSRVBridgeCacheOpGetLineSize,
					NULL, bUseLock);


	return PVRSRV_OK;
}

/*
 * Unregister all cacherangebased functions with services
 */
PVRSRV_ERROR DeinitCACHERANGEBASEDBridge(void)
{
	return PVRSRV_OK;
}
