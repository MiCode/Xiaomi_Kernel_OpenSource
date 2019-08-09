/*************************************************************************/ /*!
@File
@Title          RGX TA/3D routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX TA/3D routines
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
/* for the offsetof macro */
#include <stddef.h>

#include "pdump_km.h"
#include "pvr_debug.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgxta3d.h"
#include "rgxmem.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "ri_server.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "rgx_memallocflags.h"
#include "rgxccb.h"
#include "rgxhwperf.h"
#include "rgxtimerquery.h"
#include "rgxsyncutils.h"
#include "htbuffer.h"

#include "rgxdefs_km.h"
#include "rgx_fwif_km.h"
#include "physmem.h"
#include "sync_server.h"
#include "sync_internal.h"
#include "sync.h"
#include "process_stats.h"

#if defined(SUPPORT_BUFFER_SYNC)
#include "pvr_buffer_sync.h"
#endif

#include "sync_checkpoint.h"
#include "sync_checkpoint_internal.h"

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
#include "pvrsrv_sync_server.h"
#endif

#if defined(SUPPORT_NATIVE_FENCE_SYNC)
#include "pvr_sync.h"
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) */

#if defined(SUPPORT_PDVFS)
#include "rgxpdvfs.h"
#endif

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
#include "hash.h"
#include "rgxworkest.h"

#define HASH_CLEAN_LIMIT 6
#endif

/* Enable this to dump the compiled list of UFOs prior to kick call */
#define ENABLE_TA3D_UFO_DUMP	0

//#define TA3D_CHECKPOINT_DEBUG

#if defined(TA3D_CHECKPOINT_DEBUG)
#define CHKPT_DBG(X) PVR_DPF(X)
static INLINE
void _DebugSyncValues(const IMG_CHAR *pszFunction,
					  const IMG_UINT32 *pui32UpdateValues,
					  const IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;
	IMG_UINT32 *pui32Tmp = (IMG_UINT32*)pui32UpdateValues;

	for (i=0; i<ui32Count; i++)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: pui32IntAllocatedUpdateValues[%d](<%p>) = 0x%x", pszFunction, i, (void*)pui32Tmp, *pui32Tmp));
		pui32Tmp++;
	}
}

static INLINE
void _DebugSyncCheckpoints(const IMG_CHAR *pszFunction,
						   const IMG_CHAR *pszDMName,
						   const PSYNC_CHECKPOINT *apsSyncCheckpoints,
						   const IMG_UINT32 ui32Count)
{
	IMG_UINT32 i;
	for (i=0; i<ui32Count; i++)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: apsFence%sSyncCheckpoints[%d]=<%p>", pszFunction, pszDMName, i, *(apsSyncCheckpoints + i)));
	}
}

#else
#define CHKPT_DBG(X)
#endif

/* define the number of commands required to be set up by the CCB helper */
/* 1 command for the TA */
#define CCB_CMD_HELPER_NUM_TA_COMMANDS 1
/* Up to 3 commands for the 3D (partial render fence, partial render, and render) */
#define CCB_CMD_HELPER_NUM_3D_COMMANDS 3

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
#define WORKEST_CYCLES_PREDICTION_GET(x) ((x).ui64CyclesPrediction)
#else
#define WORKEST_CYCLES_PREDICTION_GET(x) (NO_CYCEST)
#endif

typedef struct _DEVMEM_REF_LOOKUP_
{
	IMG_UINT32 ui32ZSBufferID;
	RGX_ZSBUFFER_DATA *psZSBuffer;
} DEVMEM_REF_LOOKUP;

typedef struct _DEVMEM_FREELIST_LOOKUP_
{
	IMG_UINT32 ui32FreeListID;
	RGX_FREELIST *psFreeList;
} DEVMEM_FREELIST_LOOKUP;

typedef struct {
	DEVMEM_MEMDESC				*psContextStateMemDesc;
	RGX_SERVER_COMMON_CONTEXT	*psServerCommonContext;
	IMG_UINT32					ui32Priority;
} RGX_SERVER_RC_TA_DATA;

typedef struct {
	DEVMEM_MEMDESC				*psContextStateMemDesc;
	RGX_SERVER_COMMON_CONTEXT	*psServerCommonContext;
	IMG_UINT32					ui32Priority;
} RGX_SERVER_RC_3D_DATA;

struct _RGX_SERVER_RENDER_CONTEXT_ {
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	/* this lock protects usage of the render context.
	 * it ensures only one kick is being prepared and/or submitted on
	 * this render context at any time
	 */
	POS_LOCK				hLock;
	RGX_CCB_CMD_HELPER_DATA asTACmdHelperData[CCB_CMD_HELPER_NUM_TA_COMMANDS];
	RGX_CCB_CMD_HELPER_DATA as3DCmdHelperData[CCB_CMD_HELPER_NUM_3D_COMMANDS];
#endif
	PVRSRV_DEVICE_NODE			*psDeviceNode;
	DEVMEM_MEMDESC				*psFWRenderContextMemDesc;
	DEVMEM_MEMDESC				*psFWFrameworkMemDesc;
	RGX_SERVER_RC_TA_DATA		sTAData;
	RGX_SERVER_RC_3D_DATA		s3DData;
	IMG_UINT32					ui32CleanupStatus;
#define RC_CLEANUP_TA_COMPLETE		(1 << 0)
#define RC_CLEANUP_3D_COMPLETE		(1 << 1)
	PVRSRV_CLIENT_SYNC_PRIM		*psCleanupSync;
	DLLIST_NODE					sListNode;
	SYNC_ADDR_LIST				sSyncAddrListTAFence;
	SYNC_ADDR_LIST				sSyncAddrListTAUpdate;
	SYNC_ADDR_LIST				sSyncAddrList3DFence;
	SYNC_ADDR_LIST				sSyncAddrList3DUpdate;
	ATOMIC_T					hIntJobRef;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	WORKEST_HOST_DATA			sWorkEstData;
#endif
#if defined(SUPPORT_BUFFER_SYNC)
	struct pvr_buffer_sync_context *psBufferSyncContext;
#endif
};


/*
	Static functions used by render context code
*/

static
PVRSRV_ERROR _DestroyTAContext(RGX_SERVER_RC_TA_DATA *psTAData,
							   PVRSRV_DEVICE_NODE *psDeviceNode,
							   PVRSRV_CLIENT_SYNC_PRIM *psCleanupSync)
{
	PVRSRV_ERROR eError;

	/* Check if the FW has finished with this resource ... */
	eError = RGXFWRequestCommonContextCleanUp(psDeviceNode,
											  psTAData->psServerCommonContext,
											  psCleanupSync,
											  RGXFWIF_DM_TA,
											  PDUMP_FLAGS_NONE);
	if (eError == PVRSRV_ERROR_RETRY)
	{
		return eError;
	}
	else if (eError != PVRSRV_OK)
	{
		PVR_LOG(("%s: Unexpected error from RGXFWRequestCommonContextCleanUp (%s)",
				__FUNCTION__,
				PVRSRVGetErrorStringKM(eError)));
		return eError;
	}

	/* ... it has so we can free it's resources */
#if defined(DEBUG)
	/* Log the number of TA context stores which occurred */
	{
		RGXFWIF_TACTX_STATE	*psFWTAState;

		eError = DevmemAcquireCpuVirtAddr(psTAData->psContextStateMemDesc,
										  (void**)&psFWTAState);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"%s: Failed to map firmware render context state (%u)",
					__FUNCTION__, eError));
		}
		else
		{
			/* Release the CPU virt addr */
			DevmemReleaseCpuVirtAddr(psTAData->psContextStateMemDesc);
		}
	}
#endif
	FWCommonContextFree(psTAData->psServerCommonContext);
	DevmemFwFree(psDeviceNode->pvDevice, psTAData->psContextStateMemDesc);
	psTAData->psServerCommonContext = NULL;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR _Destroy3DContext(RGX_SERVER_RC_3D_DATA *ps3DData,
							   PVRSRV_DEVICE_NODE *psDeviceNode,
							   PVRSRV_CLIENT_SYNC_PRIM *psCleanupSync)
{
	PVRSRV_ERROR eError;

	/* Check if the FW has finished with this resource ... */
	eError = RGXFWRequestCommonContextCleanUp(psDeviceNode,
											  ps3DData->psServerCommonContext,
											  psCleanupSync,
											  RGXFWIF_DM_3D,
											  PDUMP_FLAGS_NONE);
	if (eError == PVRSRV_ERROR_RETRY)
	{
		return eError;
	}
	else if (eError != PVRSRV_OK)
	{
		PVR_LOG(("%s: Unexpected error from RGXFWRequestCommonContextCleanUp (%s)",
				 __FUNCTION__,
				 PVRSRVGetErrorStringKM(eError)));
		return eError;
	}

	/* ... it has so we can free it's resources */
#if defined(DEBUG)
	/* Log the number of 3D context stores which occurred */
	{
		RGXFWIF_3DCTX_STATE	*psFW3DState;

		eError = DevmemAcquireCpuVirtAddr(ps3DData->psContextStateMemDesc,
										  (void**)&psFW3DState);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"%s: Failed to map firmware render context state (%u)",
					__FUNCTION__, eError));
		}
		else
		{
			/* Release the CPU virt addr */
			DevmemReleaseCpuVirtAddr(ps3DData->psContextStateMemDesc);
		}
	}
#endif

	FWCommonContextFree(ps3DData->psServerCommonContext);
	DevmemFwFree(psDeviceNode->pvDevice, ps3DData->psContextStateMemDesc);
	ps3DData->psServerCommonContext = NULL;
	return PVRSRV_OK;
}

static void _RGXDumpPMRPageList(DLLIST_NODE *psNode)
{
	RGX_PMR_NODE *psPMRNode = IMG_CONTAINER_OF(psNode, RGX_PMR_NODE, sMemoryBlock);
	PVRSRV_ERROR			eError;

	eError = PMRDumpPageList(psPMRNode->psPMR,
							RGX_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Error (%u) printing pmr %p", eError, psPMRNode->psPMR));
	}
}

IMG_BOOL RGXDumpFreeListPageList(RGX_FREELIST *psFreeList)
{
	DLLIST_NODE *psNode, *psNext;

	PVR_LOG(("Freelist FWAddr 0x%08x, ID = %d, CheckSum 0x%016" IMG_UINT64_FMTSPECx,
				psFreeList->sFreeListFWDevVAddr.ui32Addr,
				psFreeList->ui32FreelistID,
				psFreeList->ui64FreelistChecksum));

	/* Dump Init FreeList page list */
	PVR_LOG(("  Initial Memory block"));
	dllist_foreach_node(&psFreeList->sMemoryBlockInitHead, psNode, psNext)
	{
		_RGXDumpPMRPageList(psNode);
	}

	/* Dump Grow FreeList page list */
	PVR_LOG(("  Grow Memory blocks"));
	dllist_foreach_node(&psFreeList->sMemoryBlockHead, psNode, psNext)
	{
		_RGXDumpPMRPageList(psNode);
	}

	return IMG_TRUE;
}

static void _CheckFreelist(RGX_FREELIST *psFreeList,
						   IMG_UINT32 ui32NumOfPagesToCheck,
						   IMG_UINT64 ui64ExpectedCheckSum,
						   IMG_UINT64 *pui64CalculatedCheckSum)
{
#if defined(NO_HARDWARE)
	/* No checksum needed as we have all information in the pdumps */
	PVR_UNREFERENCED_PARAMETER(psFreeList);
	PVR_UNREFERENCED_PARAMETER(ui32NumOfPagesToCheck);
	PVR_UNREFERENCED_PARAMETER(ui64ExpectedCheckSum);
	*pui64CalculatedCheckSum = 0;
#else
	PVRSRV_ERROR eError;
	size_t uiNumBytes;
	IMG_UINT8* pui8Buffer;
	IMG_UINT32* pui32Buffer;
	IMG_UINT32 ui32CheckSumAdd = 0;
	IMG_UINT32 ui32CheckSumXor = 0;
	IMG_UINT32 ui32Entry;
	IMG_UINT32 ui32Entry2;
	IMG_BOOL bFreelistBad = IMG_FALSE;

	*pui64CalculatedCheckSum = 0;

	PVR_ASSERT(ui32NumOfPagesToCheck <= (psFreeList->ui32CurrentFLPages + psFreeList->ui32ReadyFLPages));

	/* Allocate Buffer of the size of the freelist */
	pui8Buffer = OSAllocMem(ui32NumOfPagesToCheck * sizeof(IMG_UINT32));
	if (pui8Buffer == NULL)
	{
		PVR_LOG(("_CheckFreelist: Failed to allocate buffer to check freelist %p!", psFreeList));
		PVR_ASSERT(0);
		return;
	}

    /* Copy freelist content into Buffer */
	eError = PMR_ReadBytes(psFreeList->psFreeListPMR,
					psFreeList->uiFreeListPMROffset +
					(((psFreeList->ui32MaxFLPages -
					   psFreeList->ui32CurrentFLPages -
					   psFreeList->ui32ReadyFLPages) * sizeof(IMG_UINT32)) &
					 ~((IMG_UINT64)RGX_BIF_PM_FREELIST_BASE_ADDR_ALIGNSIZE-1)),
					pui8Buffer,
					ui32NumOfPagesToCheck * sizeof(IMG_UINT32),
					&uiNumBytes);
	if (eError != PVRSRV_OK)
	{
		OSFreeMem(pui8Buffer);
		PVR_LOG(("_CheckFreelist: Failed to get freelist data for freelist %p!", psFreeList));
		PVR_ASSERT(0);
        return;
    }

	PVR_ASSERT(uiNumBytes == ui32NumOfPagesToCheck * sizeof(IMG_UINT32));

	/* Generate checksum (skipping the first page if not allocated) */
	pui32Buffer = (IMG_UINT32 *)pui8Buffer;
	ui32Entry = ((psFreeList->ui32GrowFLPages == 0  &&  psFreeList->ui32CurrentFLPages > 1) ? 1 : 0);
	for(/*ui32Entry*/ ; ui32Entry < ui32NumOfPagesToCheck; ui32Entry++)
	{
		ui32CheckSumAdd += pui32Buffer[ui32Entry];
		ui32CheckSumXor ^= pui32Buffer[ui32Entry];

		/* Check for double entries */
		for (ui32Entry2 = ui32Entry+1; ui32Entry2 < ui32NumOfPagesToCheck; ui32Entry2++)
		{
			if (pui32Buffer[ui32Entry] == pui32Buffer[ui32Entry2])
			{
				PVR_LOG(("_CheckFreelist: Freelist consistency failure: FW addr: 0x%08X, Double entry found 0x%08x on idx: %d and %d of %d",
											psFreeList->sFreeListFWDevVAddr.ui32Addr,
											pui32Buffer[ui32Entry2],
											ui32Entry,
											ui32Entry2,
											psFreeList->ui32CurrentFLPages));
				bFreelistBad = IMG_TRUE;
				break;
			}
		}
	}

	OSFreeMem(pui8Buffer);

	/* Check the calculated checksum against the expected checksum... */
	*pui64CalculatedCheckSum = ((IMG_UINT64)ui32CheckSumXor << 32) | ui32CheckSumAdd;

	if (ui64ExpectedCheckSum != 0  &&  ui64ExpectedCheckSum != *pui64CalculatedCheckSum)
	{
		PVR_LOG(("_CheckFreelist: Checksum mismatch for freelist %p!  Expected 0x%016" IMG_UINT64_FMTSPECx " calculated 0x%016" IMG_UINT64_FMTSPECx,
		        psFreeList, ui64ExpectedCheckSum, *pui64CalculatedCheckSum));
		bFreelistBad = IMG_TRUE;
	}

	if (bFreelistBad)
	{
		PVR_LOG(("_CheckFreelist: Sleeping for ever!"));
		PVR_ASSERT(!bFreelistBad);
	}
#endif
}


/*
 *  Function to work out the number of freelist pages to reserve for growing
 *  within the FW without having to wait for the host to progress a grow
 *  request.
 * 
 *  The number of pages must be a multiple of 4 to align the PM addresses
 *  for the initial freelist allocation and also be less than the grow size.
 *
 *  If the threshold or grow size means less than 4 pages, then the feature
 *  is not used.
 */
static IMG_UINT32 _CalculateFreelistReadyPages(RGX_FREELIST *psFreeList,
                                               IMG_UINT32  ui32FLPages)
{
	IMG_UINT32  ui32ReadyFLPages = ((ui32FLPages * psFreeList->ui32GrowThreshold) / 100) &
	                               ~((RGX_BIF_PM_FREELIST_BASE_ADDR_ALIGNSIZE/sizeof(IMG_UINT32))-1);

	if (ui32ReadyFLPages > psFreeList->ui32GrowFLPages)
	{
		ui32ReadyFLPages = psFreeList->ui32GrowFLPages;
	}
	
	return ui32ReadyFLPages;
}


PVRSRV_ERROR RGXGrowFreeList(RGX_FREELIST *psFreeList,
                             IMG_UINT32 ui32NumPages,
                             PDLLIST_NODE pListHeader,
                             IMG_BOOL bForCreate)
{
	RGX_PMR_NODE	*psPMRNode;
	IMG_DEVMEM_SIZE_T uiSize;
	IMG_UINT32  ui32MappingTable = 0;
	IMG_DEVMEM_OFFSET_T uiOffset;
	IMG_DEVMEM_SIZE_T uiLength;
	IMG_DEVMEM_SIZE_T uistartPage;
	PVRSRV_ERROR eError;
	const IMG_CHAR * pszAllocName = "Free List";

	/* Are we allowed to grow ? */
	if (psFreeList->ui32MaxFLPages - (psFreeList->ui32CurrentFLPages + psFreeList->ui32ReadyFLPages) < ui32NumPages)
	{
		PVR_DPF((PVR_DBG_WARNING,"Freelist [0x%p]: grow by %u pages denied. Max PB size reached (current pages %u+%u/%u)",
				psFreeList,
				ui32NumPages,
				psFreeList->ui32CurrentFLPages,
				psFreeList->ui32ReadyFLPages,
				psFreeList->ui32MaxFLPages));
		return PVRSRV_ERROR_PBSIZE_ALREADY_MAX;
	}

	/* Allocate kernel memory block structure */
	psPMRNode = OSAllocMem(sizeof(*psPMRNode));
	if (psPMRNode == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXGrowFreeList: failed to allocate host data structure"));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorAllocHost;
	}

	/*
	 * Lock protects simultaneous manipulation of:
	 * - the memory block list
	 * - the freelist's ui32CurrentFLPages
	 */
	OSLockAcquire(psFreeList->psDevInfo->hLockFreeList);


	/* 
	 *  The PM never takes the last page in a freelist, so if this block
	 *  of pages is the first one and there is no ability to grow, then
	 *  we can skip allocating one 4K page for the lowest entry.
	 */
	psPMRNode->bFirstPageMissing = (psFreeList->ui32GrowFLPages == 0  &&  ui32NumPages > 1);
	psPMRNode->ui32NumPages = ui32NumPages;
	psPMRNode->psFreeList = psFreeList;

	/* Allocate Memory Block */
	PDUMPCOMMENT("Allocate PB Block (Pages %08X)", ui32NumPages);
	uiSize = (IMG_DEVMEM_SIZE_T)ui32NumPages * RGX_BIF_PM_PHYSICAL_PAGE_SIZE;
	if (psPMRNode->bFirstPageMissing)
	{
		uiSize -= RGX_BIF_PM_PHYSICAL_PAGE_SIZE;
	}
	eError = PhysmemNewRamBackedPMR(NULL,
	                                psFreeList->psDevInfo->psDeviceNode,
									uiSize,
									uiSize,
									1,
									1,
									&ui32MappingTable,
									RGX_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT,
									PVRSRV_MEMALLOCFLAG_GPU_READABLE,
									OSStringLength(pszAllocName) + 1,
									pszAllocName,
									psFreeList->ownerPid,
									&psPMRNode->psPMR);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "RGXGrowFreeList: Failed to allocate PB block of size: 0x%016" IMG_UINT64_FMTSPECX,
				 (IMG_UINT64)uiSize));
		goto ErrorBlockAlloc;
	}

	/* Zeroing physical pages pointed by the PMR */
	if (psFreeList->psDevInfo->ui32DeviceFlags & RGXKM_DEVICE_STATE_ZERO_FREELIST)
	{
		eError = PMRZeroingPMR(psPMRNode->psPMR, RGX_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXGrowFreeList: Failed to zero PMR %p of freelist %p with Error %d",
									psPMRNode->psPMR,
									psFreeList,
									eError));
			PVR_ASSERT(0);
		}
	}

	uiLength = psPMRNode->ui32NumPages * sizeof(IMG_UINT32);
	uistartPage = (psFreeList->ui32MaxFLPages - psFreeList->ui32CurrentFLPages - psPMRNode->ui32NumPages);
	uiOffset = psFreeList->uiFreeListPMROffset + ((uistartPage * sizeof(IMG_UINT32)) & ~((IMG_UINT64)RGX_BIF_PM_FREELIST_BASE_ADDR_ALIGNSIZE-1));

#if defined(PVR_RI_DEBUG)

	eError = RIWritePMREntryWithOwnerKM(psPMRNode->psPMR,
                                        psFreeList->ownerPid);
	if ( eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: call to RIWritePMREntryWithOwnerKM failed (eError=%d)",
				__func__,
				eError));
	}

	 /* Attach RI information */
	eError = RIWriteMEMDESCEntryKM(psPMRNode->psPMR,
	                               OSStringNLength(pszAllocName, DEVMEM_ANNOTATION_MAX_LEN),
	                               pszAllocName,
	                               0,
	                               uiSize,
	                               IMG_FALSE,
	                               IMG_FALSE,
	                               &psPMRNode->hRIHandle);
	if ( eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: call to RIWriteMEMDESCEntryKM failed (eError=%d)",
				__func__,
				eError));
	}

#endif /* if defined(PVR_RI_DEBUG) */

	/* write Freelist with Memory Block physical addresses */
	eError = PMRWritePMPageList(
						/* Target PMR, offset, and length */
						psFreeList->psFreeListPMR,
						(psPMRNode->bFirstPageMissing ? uiOffset + sizeof(IMG_UINT32) : uiOffset),
						(psPMRNode->bFirstPageMissing ? uiLength - sizeof(IMG_UINT32) : uiLength),
						/* Referenced PMR, and "page" granularity */
						psPMRNode->psPMR,
						RGX_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT,
						&psPMRNode->psPageList);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "RGXGrowFreeList: Failed to write pages of Node %p",
				 psPMRNode));
		goto ErrorPopulateFreelist;
	}

#if defined(SUPPORT_SHADOW_FREELISTS)
	/* Copy freelist memory to shadow freelist */
	{
		const IMG_UINT32 ui32FLMaxSize = psFreeList->ui32MaxFLPages * sizeof (IMG_UINT32);
		const IMG_UINT32 ui32MapSize = ui32FLMaxSize * 2;
		const IMG_UINT32 ui32CopyOffset = uiOffset - psFreeList->uiFreeListPMROffset;
		IMG_BYTE *pFLMapAddr;
		size_t uiNumBytes;
		PVRSRV_ERROR res;
		IMG_HANDLE hMapHandle;

		/* Map both the FL and the shadow FL */
		res = PMRAcquireKernelMappingData(psFreeList->psFreeListPMR, psFreeList->uiFreeListPMROffset, ui32MapSize,
		                                  (void**) &pFLMapAddr, &uiNumBytes, &hMapHandle);
		if (res != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "RGXGrowFreeList: Failed to map freelist (ID=%d)",
			         psFreeList->ui32FreelistID));
			goto ErrorPopulateFreelist;
		}

		/* Copy only the newly added memory */
		memcpy(pFLMapAddr + ui32FLMaxSize + ui32CopyOffset, pFLMapAddr + ui32CopyOffset , uiLength);

#if defined(PDUMP)
		PDUMPCOMMENT("Initialize shadow freelist");

		/* Translate memcpy to pdump */
		{
			IMG_DEVMEM_OFFSET_T uiCurrOffset;

			for (uiCurrOffset = uiOffset; (uiCurrOffset - uiOffset) < uiLength; uiCurrOffset += sizeof (IMG_UINT32))
			{
				PMRPDumpCopyMem32(psFreeList->psFreeListPMR,
				                  uiCurrOffset + ui32FLMaxSize,
				                  psFreeList->psFreeListPMR,
				                  uiCurrOffset,
				                  ":SYSMEM:$1",
				                  PDUMP_FLAGS_CONTINUOUS);
			}
		}
#endif


		res = PMRReleaseKernelMappingData(psFreeList->psFreeListPMR, hMapHandle);

		if (res != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "RGXGrowFreeList: Failed to release freelist mapping (ID=%d)",
			         psFreeList->ui32FreelistID));
			goto ErrorPopulateFreelist;
		}
	}
#endif

	/* We add It must be added to the tail, otherwise the freelist population won't work */
	dllist_add_to_head(pListHeader, &psPMRNode->sMemoryBlock);

	/* Update number of available pages */
	psFreeList->ui32CurrentFLPages += ui32NumPages;

	/* Reserve a number ready pages to allow the FW to process OOM quickly and asynchronously request a grow. */
	psFreeList->ui32ReadyFLPages    = _CalculateFreelistReadyPages(psFreeList, psFreeList->ui32CurrentFLPages);
	psFreeList->ui32CurrentFLPages -= psFreeList->ui32ReadyFLPages;

	/* Update statistics */
	if (psFreeList->ui32NumHighPages < psFreeList->ui32CurrentFLPages)
	{
		psFreeList->ui32NumHighPages = psFreeList->ui32CurrentFLPages;
	}

	if (psFreeList->bCheckFreelist)
	{
		/*
		 *  We can only calculate the freelist checksum when the list is full
		 *  (e.g. at initial creation time). At other times the checksum cannot
		 *  be calculated and has to be disabled for this freelist.
		 */
		if ((psFreeList->ui32CurrentFLPages + psFreeList->ui32ReadyFLPages) == ui32NumPages)
		{
			_CheckFreelist(psFreeList, ui32NumPages, 0, &psFreeList->ui64FreelistChecksum);
		}
		else
		{
			psFreeList->ui64FreelistChecksum = 0;
		}
	}
	OSLockRelease(psFreeList->psDevInfo->hLockFreeList);

	PVR_DPF((PVR_DBG_MESSAGE,"Freelist [%p]: %s %u pages (pages=%u+%u/%u checksum=0x%016" IMG_UINT64_FMTSPECx "%s)",
			psFreeList,
	        ((psFreeList->ui32CurrentFLPages + psFreeList->ui32ReadyFLPages) == ui32NumPages ? "Create initial" : "Grow by"),
			ui32NumPages,
			psFreeList->ui32CurrentFLPages,
			psFreeList->ui32ReadyFLPages,
			psFreeList->ui32MaxFLPages,
			psFreeList->ui64FreelistChecksum,
			(psPMRNode->bFirstPageMissing ? " - lowest page not allocated" : "")));

	return PVRSRV_OK;

	/* Error handling */
ErrorPopulateFreelist:
	PMRUnrefPMR(psPMRNode->psPMR);

ErrorBlockAlloc:
	OSFreeMem(psPMRNode);
	OSLockRelease(psFreeList->psDevInfo->hLockFreeList);

ErrorAllocHost:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;

}

static PVRSRV_ERROR RGXShrinkFreeList(PDLLIST_NODE pListHeader,
										RGX_FREELIST *psFreeList)
{
	DLLIST_NODE *psNode;
	RGX_PMR_NODE *psPMRNode;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 ui32OldValue;

	/*
	 * Lock protects simultaneous manipulation of:
	 * - the memory block list
	 * - the freelist's ui32CurrentFLPages value
	 */
	PVR_ASSERT(pListHeader);
	PVR_ASSERT(psFreeList);
	PVR_ASSERT(psFreeList->psDevInfo);
	PVR_ASSERT(psFreeList->psDevInfo->hLockFreeList);

	OSLockAcquire(psFreeList->psDevInfo->hLockFreeList);

	/* Get node from head of list and remove it */
	psNode = dllist_get_next_node(pListHeader);
	if (psNode)
	{
		dllist_remove_node(psNode);

		psPMRNode = IMG_CONTAINER_OF(psNode, RGX_PMR_NODE, sMemoryBlock);
		PVR_ASSERT(psPMRNode);
		PVR_ASSERT(psPMRNode->psPMR);
		PVR_ASSERT(psPMRNode->psFreeList);

		/* remove block from freelist list */

		/* Unwrite Freelist with Memory Block physical addresses */
		eError = PMRUnwritePMPageList(psPMRNode->psPageList);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "RGXRemoveBlockFromFreeListKM: Failed to unwrite pages of Node %p",
					 psPMRNode));
			PVR_ASSERT(IMG_FALSE);
		}

#if defined(PVR_RI_DEBUG)

		if (psPMRNode->hRIHandle)
		{
		    PVRSRV_ERROR eError;

		    eError = RIDeleteMEMDESCEntryKM(psPMRNode->hRIHandle);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: call to RIDeleteMEMDESCEntryKM failed (eError=%d)", __func__, eError));
			}
		}

#endif  /* if defined(PVR_RI_DEBUG) */

		/* Free PMR (We should be the only one that holds a ref on the PMR) */
		eError = PMRUnrefPMR(psPMRNode->psPMR);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "RGXRemoveBlockFromFreeListKM: Failed to free PB block %p (error %u)",
					 psPMRNode->psPMR,
					 eError));
			PVR_ASSERT(IMG_FALSE);
		}

		/* update available pages in freelist */
		ui32OldValue = psFreeList->ui32CurrentFLPages + psFreeList->ui32ReadyFLPages;

		/*
		 * Deallocated pages should first be deducted from ReadyPages bank, once
		 * there are no more left, start deducting them from CurrentPage bank.
		 */
		if (psPMRNode->ui32NumPages > psFreeList->ui32ReadyFLPages)
		{
			psFreeList->ui32CurrentFLPages -= psPMRNode->ui32NumPages - psFreeList->ui32ReadyFLPages;
			psFreeList->ui32ReadyFLPages = 0;
		}
		else
		{
			psFreeList->ui32ReadyFLPages -= psPMRNode->ui32NumPages;
		}

		/* check underflow */
		PVR_ASSERT(ui32OldValue > (psFreeList->ui32CurrentFLPages + psFreeList->ui32ReadyFLPages));

		PVR_DPF((PVR_DBG_MESSAGE, "Freelist [%p]: shrink by %u pages (current pages %u/%u)",
								psFreeList,
								psPMRNode->ui32NumPages,
								psFreeList->ui32CurrentFLPages,
								psFreeList->ui32MaxFLPages));

		OSFreeMem(psPMRNode);
	}
	else
	{
		PVR_DPF((PVR_DBG_WARNING,"Freelist [0x%p]: shrink denied. PB already at initial PB size (%u pages)",
								psFreeList,
								psFreeList->ui32InitFLPages));
		eError = PVRSRV_ERROR_PBSIZE_ALREADY_MIN;
	}

	OSLockRelease(psFreeList->psDevInfo->hLockFreeList);

	return eError;
}

static RGX_FREELIST *FindFreeList(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_UINT32 ui32FreelistID)
{
	DLLIST_NODE *psNode, *psNext;
	RGX_FREELIST *psFreeList = NULL;

	OSLockAcquire(psDevInfo->hLockFreeList);

	dllist_foreach_node(&psDevInfo->sFreeListHead, psNode, psNext)
	{
		RGX_FREELIST *psThisFreeList = IMG_CONTAINER_OF(psNode, RGX_FREELIST, sNode);

		if (psThisFreeList->ui32FreelistID == ui32FreelistID)
		{
			psFreeList = psThisFreeList;
			break;
		}
	}

	OSLockRelease(psDevInfo->hLockFreeList);
	return psFreeList;
}

void RGXProcessRequestGrow(PVRSRV_RGXDEV_INFO *psDevInfo,
						   IMG_UINT32 ui32FreelistID)
{
	RGX_FREELIST *psFreeList = NULL;
	RGXFWIF_KCCB_CMD s3DCCBCmd;
	IMG_UINT32 ui32GrowValue;
	PVRSRV_ERROR eError;

	PVR_ASSERT(psDevInfo);

	psFreeList = FindFreeList(psDevInfo, ui32FreelistID);

	if (psFreeList)
	{
		/* Since the FW made the request, it has already consumed the ready pages, update the host struct */
		psFreeList->ui32CurrentFLPages += psFreeList->ui32ReadyFLPages;
		psFreeList->ui32ReadyFLPages = 0;

		/* Try to grow the freelist */
		eError = RGXGrowFreeList(psFreeList,
		                         psFreeList->ui32GrowFLPages,
		                         &psFreeList->sMemoryBlockHead,
		                         IMG_FALSE);

		if (eError == PVRSRV_OK)
		{
			/* Grow successful, return size of grow size */
			ui32GrowValue = psFreeList->ui32GrowFLPages;

			psFreeList->ui32NumGrowReqByFW++;

 #if defined(PVRSRV_ENABLE_PROCESS_STATS)
			/* Update Stats */
			PVRSRVStatsUpdateFreelistStats(0,
	                               1, /* Add 1 to the appropriate counter (Requests by FW) */
	                               psFreeList->ui32InitFLPages,
	                               psFreeList->ui32NumHighPages,
	                               psFreeList->ownerPid);

 #endif

		}
		else
		{
			/* Grow failed */
			ui32GrowValue = 0;
			PVR_DPF((PVR_DBG_ERROR,"Grow for FreeList %p failed (error %u)",
									psFreeList,
									eError));
		}

		/* send feedback */
		s3DCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_FREELIST_GROW_UPDATE;
		s3DCCBCmd.uCmdData.sFreeListGSData.sFreeListFWDevVAddr.ui32Addr = psFreeList->sFreeListFWDevVAddr.ui32Addr;
		s3DCCBCmd.uCmdData.sFreeListGSData.ui32DeltaPages = ui32GrowValue;
		s3DCCBCmd.uCmdData.sFreeListGSData.ui32NewPages = psFreeList->ui32CurrentFLPages + psFreeList->ui32ReadyFLPages;
		s3DCCBCmd.uCmdData.sFreeListGSData.ui32ReadyPages = psFreeList->ui32ReadyFLPages;
			

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError = RGXScheduleCommand(psDevInfo,
										RGXFWIF_DM_3D,
										&s3DCCBCmd,
										sizeof(s3DCCBCmd),
										0,
										PDUMP_FLAGS_NONE);
			if (eError != PVRSRV_ERROR_RETRY)
			{
				break;
			}
			OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();
		/* Kernel CCB should never fill up, as the FW is processing them right away  */

		PVR_ASSERT(eError == PVRSRV_OK);
	}
	else
	{
		/* Should never happen */
		PVR_DPF((PVR_DBG_ERROR,"FreeList Lookup for FreeList ID 0x%08x failed (Populate)", ui32FreelistID));
		PVR_ASSERT(IMG_FALSE);
	}
}

static void _RGXFreeListReconstruction(PDLLIST_NODE psNode)
{

	PVRSRV_RGXDEV_INFO 		*psDevInfo;
	RGX_FREELIST			*psFreeList;
	RGX_PMR_NODE			*psPMRNode;
	PVRSRV_ERROR			eError;
	IMG_DEVMEM_OFFSET_T		uiOffset;
	IMG_DEVMEM_SIZE_T		uiLength;
	IMG_UINT32				ui32StartPage;

	psPMRNode = IMG_CONTAINER_OF(psNode, RGX_PMR_NODE, sMemoryBlock);
	psFreeList = psPMRNode->psFreeList;
	PVR_ASSERT(psFreeList);
	psDevInfo = psFreeList->psDevInfo;
	PVR_ASSERT(psDevInfo);

	uiLength = psPMRNode->ui32NumPages * sizeof(IMG_UINT32);
	ui32StartPage = (psFreeList->ui32MaxFLPages - psFreeList->ui32CurrentFLPages - psPMRNode->ui32NumPages);
	uiOffset = psFreeList->uiFreeListPMROffset + ((ui32StartPage * sizeof(IMG_UINT32)) & ~((IMG_UINT64)RGX_BIF_PM_FREELIST_BASE_ADDR_ALIGNSIZE-1));
	
	PMRUnwritePMPageList(psPMRNode->psPageList);
	psPMRNode->psPageList = NULL;
	eError = PMRWritePMPageList(
						/* Target PMR, offset, and length */
						psFreeList->psFreeListPMR,
						(psPMRNode->bFirstPageMissing ? uiOffset + sizeof(IMG_UINT32) : uiOffset),
						(psPMRNode->bFirstPageMissing ? uiLength - sizeof(IMG_UINT32) : uiLength),
						/* Referenced PMR, and "page" granularity */
						psPMRNode->psPMR,
						RGX_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT,
						&psPMRNode->psPageList);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Error (%u) writing FL 0x%08x", eError, (IMG_UINT32)psFreeList->ui32FreelistID));
	}

	/* Zeroing physical pages pointed by the reconstructed freelist */
	if (psDevInfo->ui32DeviceFlags & RGXKM_DEVICE_STATE_ZERO_FREELIST)
	{
		eError = PMRZeroingPMR(psPMRNode->psPMR, RGX_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"_RGXFreeListReconstruction: Failed to zero PMR %p of freelist %p with Error %d",
									psPMRNode->psPMR,
									psFreeList,
									eError));
			PVR_ASSERT(0);
		}
	}


	psFreeList->ui32CurrentFLPages += psPMRNode->ui32NumPages;
}


static PVRSRV_ERROR RGXReconstructFreeList(RGX_FREELIST *psFreeList)
{
	IMG_UINT32        ui32OriginalFLPages;
	DLLIST_NODE       *psNode, *psNext;
	RGXFWIF_FREELIST  *psFWFreeList;
	PVRSRV_ERROR      eError;

	//PVR_DPF((PVR_DBG_ERROR, "FreeList RECONSTRUCTION: Reconstructing freelist %p (ID=%u)", psFreeList, psFreeList->ui32FreelistID));

	/* Do the FreeList Reconstruction */
	ui32OriginalFLPages            = psFreeList->ui32CurrentFLPages;
	psFreeList->ui32CurrentFLPages = 0;

	/* Reconstructing Init FreeList pages */
	dllist_foreach_node(&psFreeList->sMemoryBlockInitHead, psNode, psNext)
	{
		_RGXFreeListReconstruction(psNode);
	}

	/* Reconstructing Grow FreeList pages */
	dllist_foreach_node(&psFreeList->sMemoryBlockHead, psNode, psNext)
	{
		_RGXFreeListReconstruction(psNode);
	}
	
	/* Ready pages are allocated but kept hidden until OOM occurs. */
	psFreeList->ui32CurrentFLPages -= psFreeList->ui32ReadyFLPages;
	if (psFreeList->ui32CurrentFLPages != ui32OriginalFLPages)
	{
		PVR_ASSERT(psFreeList->ui32CurrentFLPages == ui32OriginalFLPages);
		return PVRSRV_ERROR_FREELIST_RECONSTRUCTION_FAILED;
	}

	/* Reset the firmware freelist structure */
	eError = DevmemAcquireCpuVirtAddr(psFreeList->psFWFreelistMemDesc, (void **)&psFWFreeList);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	psFWFreeList->ui32CurrentStackTop       = psFWFreeList->ui32CurrentPages - 1;
	psFWFreeList->ui32AllocatedPageCount    = 0;
	psFWFreeList->ui32AllocatedMMUPageCount = 0;
	psFWFreeList->ui32HWRCounter++;

	DevmemReleaseCpuVirtAddr(psFreeList->psFWFreelistMemDesc);

	/* Check the Freelist checksum if required (as the list is fully populated) */
	if (psFreeList->bCheckFreelist)
	{
		IMG_UINT64  ui64CheckSum;

		_CheckFreelist(psFreeList, psFreeList->ui32CurrentFLPages, psFreeList->ui64FreelistChecksum, &ui64CheckSum);
	}

	return eError;
}


void RGXProcessRequestFreelistsReconstruction(PVRSRV_RGXDEV_INFO *psDevInfo,
                                              IMG_UINT32 ui32FreelistsCount,
                                              IMG_UINT32 *paui32Freelists)
{
	PVRSRV_ERROR      eError = PVRSRV_OK;
	DLLIST_NODE       *psNode, *psNext;
	IMG_UINT32        ui32Loop;
	RGXFWIF_KCCB_CMD  sTACCBCmd;

	PVR_ASSERT(psDevInfo != NULL);
	PVR_ASSERT(ui32FreelistsCount <= (MAX_HW_TA3DCONTEXTS * RGXFW_MAX_FREELISTS));

	//PVR_DPF((PVR_DBG_ERROR, "FreeList RECONSTRUCTION: %u freelist(s) requested for reconstruction", ui32FreelistsCount));

	/*
	 *  Initialise the response command (in case we don't find a freelist ID)...
	 */
	sTACCBCmd.eCmdType = RGXFWIF_KCCB_CMD_FREELISTS_RECONSTRUCTION_UPDATE;
	sTACCBCmd.uCmdData.sFreeListsReconstructionData.ui32FreelistsCount = ui32FreelistsCount;

	for (ui32Loop = 0; ui32Loop < ui32FreelistsCount; ui32Loop++)
	{
		sTACCBCmd.uCmdData.sFreeListsReconstructionData.aui32FreelistIDs[ui32Loop] = paui32Freelists[ui32Loop] |
		                                                                             RGXFWIF_FREELISTS_RECONSTRUCTION_FAILED_FLAG;
	}

	/*
	 *  The list of freelists we have been given for reconstruction will
	 *  consist of local and global freelists (maybe MMU as well). Any
	 *  local freelists will have their global list specified as well.
	 *  However there may be other local freelists not listed, which are
	 *  going to have their global freelist reconstructed. Therefore we
	 *  have to find those freelists as well meaning we will have to
	 *  iterate the entire list of freelists to find which must be reconstructed.
	 */
	OSLockAcquire(psDevInfo->hLockFreeList);
	dllist_foreach_node(&psDevInfo->sFreeListHead, psNode, psNext)
	{
		RGX_FREELIST  *psFreeList  = IMG_CONTAINER_OF(psNode, RGX_FREELIST, sNode);
		IMG_BOOL      bReconstruct = IMG_FALSE;

		/*
		 *  Check if this freelist needs to be reconstructed (was it requested
		 *  or was its global freelist requested)...
		 */
		for (ui32Loop = 0; ui32Loop < ui32FreelistsCount; ui32Loop++)
		{
			if (paui32Freelists[ui32Loop] == psFreeList->ui32FreelistID  ||
			    paui32Freelists[ui32Loop] == psFreeList->ui32FreelistGlobalID)
			{
				bReconstruct = IMG_TRUE;
				break;
			}
		}

		if (bReconstruct)
		{
			eError = RGXReconstructFreeList(psFreeList);
			if (eError == PVRSRV_OK)
			{
				for (ui32Loop = 0; ui32Loop < ui32FreelistsCount; ui32Loop++)
				{
					if (paui32Freelists[ui32Loop] == psFreeList->ui32FreelistID)
					{
						/* Reconstruction of this requested freelist was successful... */
						sTACCBCmd.uCmdData.sFreeListsReconstructionData.aui32FreelistIDs[ui32Loop] &= ~RGXFWIF_FREELISTS_RECONSTRUCTION_FAILED_FLAG;
						break;
					}
				}
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR,"Reconstructing of FreeList %p failed (error %u)",
						 psFreeList,
						 eError));
			}
		}
	}
	OSLockRelease(psDevInfo->hLockFreeList);

	/* Check that all freelists were found and reconstructed... */
	for (ui32Loop = 0; ui32Loop < ui32FreelistsCount; ui32Loop++)
	{
		PVR_ASSERT((sTACCBCmd.uCmdData.sFreeListsReconstructionData.aui32FreelistIDs[ui32Loop] &
		            RGXFWIF_FREELISTS_RECONSTRUCTION_FAILED_FLAG) == 0);
	}

	/* send feedback */
	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psDevInfo,
		                            RGXFWIF_DM_TA,
		                            &sTACCBCmd,
		                            sizeof(sTACCBCmd),
		                            0,
		                            PDUMP_FLAGS_NONE);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	/* Kernel CCB should never fill up, as the FW is processing them right away  */
	PVR_ASSERT(eError == PVRSRV_OK);
}

/* Create HWRTDataSet */
PVRSRV_ERROR RGXCreateHWRTData(CONNECTION_DATA      *psConnection,
                               PVRSRV_DEVICE_NODE	*psDeviceNode,
							   IMG_UINT32			psRenderTarget, /* FIXME this should not be IMG_UINT32 */
							   IMG_DEV_VIRTADDR		psPMMListDevVAddr,
							   RGX_FREELIST			*apsFreeLists[RGXFW_MAX_FREELISTS],
							   RGX_RTDATA_CLEANUP_DATA	**ppsCleanupData,
							   DEVMEM_MEMDESC		**ppsRTACtlMemDesc,
							   IMG_UINT32           ui32PPPScreen,
							   IMG_UINT32           ui32PPPGridOffset,
							   IMG_UINT64           ui64PPPMultiSampleCtl,
							   IMG_UINT32           ui32TPCStride,
							   IMG_DEV_VIRTADDR		sTailPtrsDevVAddr,
							   IMG_UINT32           ui32TPCSize,
							   IMG_UINT32           ui32TEScreen,
							   IMG_UINT32           ui32TEAA,
							   IMG_UINT32           ui32TEMTILE1,
							   IMG_UINT32           ui32TEMTILE2,
							   IMG_UINT32           ui32MTileStride,
							   IMG_UINT32                 ui32ISPMergeLowerX,
							   IMG_UINT32                 ui32ISPMergeLowerY,
							   IMG_UINT32                 ui32ISPMergeUpperX,
							   IMG_UINT32                 ui32ISPMergeUpperY,
							   IMG_UINT32                 ui32ISPMergeScaleX,
							   IMG_UINT32                 ui32ISPMergeScaleY,
							   IMG_UINT16			ui16MaxRTs,
							   DEVMEM_MEMDESC		**ppsMemDesc,
							   IMG_UINT32			*puiHWRTData)
{
	PVRSRV_ERROR eError;
	PVRSRV_RGXDEV_INFO *psDevInfo;
	RGXFWIF_DEV_VIRTADDR pFirmwareAddr;
	RGXFWIF_HWRTDATA *psHWRTData;
	RGXFWIF_RTA_CTL *psRTACtl;
	IMG_UINT32 ui32Loop;
	RGX_RTDATA_CLEANUP_DATA *psTmpCleanup;

	PVR_UNREFERENCED_PARAMETER(psConnection);

	/* Prepare cleanup struct */
	psTmpCleanup = OSAllocZMem(sizeof(*psTmpCleanup));
	if (psTmpCleanup == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto AllocError;
	}

	*ppsCleanupData = psTmpCleanup;

	/* Allocate cleanup sync */
	eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
						   &psTmpCleanup->psCleanupSync,
						   "HWRTData cleanup");
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXCreateHWRTData: Failed to allocate cleanup sync (0x%x)",
				eError));
		goto SyncAlloc;
	}

	psDevInfo = psDeviceNode->pvDevice;

	/*
	 * This FW RT-Data is only mapped into kernel for initialisation.
	 * Otherwise this allocation is only used by the FW.
	 * Therefore the GPU cache doesn't need coherency,
	 * and write-combine is suffice on the CPU side (WC buffer will be flushed at the first TA-kick)
	 */
	eError = DevmemFwAllocate(psDevInfo,
							sizeof(RGXFWIF_HWRTDATA),
							PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
							PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
							PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
							PVRSRV_MEMALLOCFLAG_GPU_READABLE |
							PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
							PVRSRV_MEMALLOCFLAG_GPU_CACHE_INCOHERENT |
							PVRSRV_MEMALLOCFLAG_CPU_READABLE |
							PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
							PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
							PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE,
							"FwHWRTData",
							ppsMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXCreateHWRTData: DevmemAllocate for RGX_FWIF_HWRTDATA failed"));
		goto FWRTDataAllocateError;
	}

	psTmpCleanup->psDeviceNode = psDeviceNode;
	psTmpCleanup->psFWHWRTDataMemDesc = *ppsMemDesc;

	RGXSetFirmwareAddress(&pFirmwareAddr, *ppsMemDesc, 0, RFW_FWADDR_FLAG_NONE);

	*puiHWRTData = pFirmwareAddr.ui32Addr;

	eError = DevmemAcquireCpuVirtAddr(*ppsMemDesc, (void **)&psHWRTData);
	PVR_LOGG_IF_ERROR(eError, "Devmem AcquireCpuVirtAddr", FWRTDataCpuMapError);

	/* FIXME: MList is something that that PM writes physical addresses to,
	 * so ideally its best allocated in kernel */
	psHWRTData->psPMMListDevVAddr = psPMMListDevVAddr;
	psHWRTData->psParentRenderTarget.ui32Addr = psRenderTarget;

	psHWRTData->ui32PPPScreen         = ui32PPPScreen;
	psHWRTData->ui32PPPGridOffset     = ui32PPPGridOffset;
	psHWRTData->ui64PPPMultiSampleCtl = ui64PPPMultiSampleCtl;
	psHWRTData->ui32TPCStride         = ui32TPCStride;
	psHWRTData->sTailPtrsDevVAddr     = sTailPtrsDevVAddr;
	psHWRTData->ui32TPCSize           = ui32TPCSize;
	psHWRTData->ui32TEScreen          = ui32TEScreen;
	psHWRTData->ui32TEAA              = ui32TEAA;
	psHWRTData->ui32TEMTILE1          = ui32TEMTILE1;
	psHWRTData->ui32TEMTILE2          = ui32TEMTILE2;
	psHWRTData->ui32MTileStride       = ui32MTileStride;
	psHWRTData->ui32ISPMergeLowerX = ui32ISPMergeLowerX;
	psHWRTData->ui32ISPMergeLowerY = ui32ISPMergeLowerY;
	psHWRTData->ui32ISPMergeUpperX = ui32ISPMergeUpperX;
	psHWRTData->ui32ISPMergeUpperY = ui32ISPMergeUpperY;
	psHWRTData->ui32ISPMergeScaleX = ui32ISPMergeScaleX;
	psHWRTData->ui32ISPMergeScaleY = ui32ISPMergeScaleY;

	OSLockAcquire(psDevInfo->hLockFreeList);
	for (ui32Loop = 0; ui32Loop < RGXFW_MAX_FREELISTS; ui32Loop++)
	{
		psTmpCleanup->apsFreeLists[ui32Loop] = apsFreeLists[ui32Loop];
		psTmpCleanup->apsFreeLists[ui32Loop]->ui32RefCount++;
		psHWRTData->apsFreeLists[ui32Loop].ui32Addr = psTmpCleanup->apsFreeLists[ui32Loop]->sFreeListFWDevVAddr.ui32Addr;
		/* invalid initial snapshot value, the snapshot is always taken during first kick
		 * and hence the value get replaced during the first kick anyway. So it's safe to set it 0.
		*/
		psHWRTData->aui32FreeListHWRSnapshot[ui32Loop] = 0;
	}
	OSLockRelease(psDevInfo->hLockFreeList);

	PDUMPCOMMENT("Allocate RGXFW RTA control");
	eError = DevmemFwAllocate(psDevInfo,
										sizeof(RGXFWIF_RTA_CTL),
										PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
										PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
										PVRSRV_MEMALLOCFLAG_GPU_READABLE |
										PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
										PVRSRV_MEMALLOCFLAG_UNCACHED |
										PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC,
										"FwRTAControl",
										ppsRTACtlMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXCreateHWRTData: Failed to allocate RGX RTA control (%u)",
				eError));
		goto FWRTAAllocateError;
	}
	psTmpCleanup->psRTACtlMemDesc = *ppsRTACtlMemDesc;
	RGXSetFirmwareAddress(&psHWRTData->psRTACtl,
								   *ppsRTACtlMemDesc,
								   0, RFW_FWADDR_FLAG_NONE);

	eError = DevmemAcquireCpuVirtAddr(*ppsRTACtlMemDesc, (void **)&psRTACtl);
	PVR_LOGG_IF_ERROR(eError, "Devmem AcquireCpuVirtAddr", FWRTACpuMapError);
	psRTACtl->ui32RenderTargetIndex = 0;
	psRTACtl->ui32ActiveRenderTargets = 0;

	if (ui16MaxRTs > 1)
	{
		/* Allocate memory for the checks */
		PDUMPCOMMENT("Allocate memory for shadow render target cache");
		eError = DevmemFwAllocate(psDevInfo,
								ui16MaxRTs * sizeof(IMG_UINT32),
								PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
								PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
								PVRSRV_MEMALLOCFLAG_GPU_READABLE |
								PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
								PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
								PVRSRV_MEMALLOCFLAG_UNCACHED|
								PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC,
								"FwShadowRTCache",
								&psTmpCleanup->psRTArrayMemDesc);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXCreateHWRTData: Failed to allocate %d bytes for render target array (%u)",
				ui16MaxRTs, eError));
			goto FWAllocateRTArryError;
		}

		RGXSetFirmwareAddress(&psRTACtl->sValidRenderTargets,
										psTmpCleanup->psRTArrayMemDesc,
										0, RFW_FWADDR_FLAG_NONE);

		/* Allocate memory for the checks */
		PDUMPCOMMENT("Allocate memory for tracking renders accumulation");
		eError = DevmemFwAllocate(psDevInfo,
                                                        ui16MaxRTs * sizeof(IMG_UINT32),
                                                        PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
							PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
                                                        PVRSRV_MEMALLOCFLAG_GPU_READABLE |
                                                        PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
                                                        PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
                                                        PVRSRV_MEMALLOCFLAG_UNCACHED|
                                                        PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC,
                                                        "FwRendersAccumulation",
                                                        &psTmpCleanup->psRendersAccArrayMemDesc);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXCreateHWRTData: Failed to allocate %d bytes for render target array (%u) (renders accumulation)",
						  ui16MaxRTs, eError));
			goto FWAllocateRTAccArryError;
		}

		RGXSetFirmwareAddress(&psRTACtl->sNumRenders,
                                                psTmpCleanup->psRendersAccArrayMemDesc,
                                                0, RFW_FWADDR_FLAG_NONE);
		psRTACtl->ui16MaxRTs = ui16MaxRTs;
	}
	else
	{
		psRTACtl->sValidRenderTargets.ui32Addr = 0;
		psRTACtl->sNumRenders.ui32Addr = 0;
		psRTACtl->ui16MaxRTs = 1;
	}

	PDUMPCOMMENT("Dump HWRTData 0x%08X", *puiHWRTData);
	DevmemPDumpLoadMem(*ppsMemDesc, 0, sizeof(*psHWRTData), PDUMP_FLAGS_CONTINUOUS);
	PDUMPCOMMENT("Dump RTACtl");
	DevmemPDumpLoadMem(*ppsRTACtlMemDesc, 0, sizeof(*psRTACtl), PDUMP_FLAGS_CONTINUOUS);

	DevmemReleaseCpuVirtAddr(*ppsMemDesc);
	DevmemReleaseCpuVirtAddr(*ppsRTACtlMemDesc);
	return PVRSRV_OK;

FWAllocateRTAccArryError:
	DevmemFwFree(psDevInfo, psTmpCleanup->psRTArrayMemDesc);
FWAllocateRTArryError:
	DevmemReleaseCpuVirtAddr(*ppsRTACtlMemDesc);
FWRTACpuMapError:
	RGXUnsetFirmwareAddress(*ppsRTACtlMemDesc);
	DevmemFwFree(psDevInfo, *ppsRTACtlMemDesc);
FWRTAAllocateError:
	OSLockAcquire(psDevInfo->hLockFreeList);
	for (ui32Loop = 0; ui32Loop < RGXFW_MAX_FREELISTS; ui32Loop++)
	{
		PVR_ASSERT(psTmpCleanup->apsFreeLists[ui32Loop]->ui32RefCount > 0);
		psTmpCleanup->apsFreeLists[ui32Loop]->ui32RefCount--;
	}
	OSLockRelease(psDevInfo->hLockFreeList);
	DevmemReleaseCpuVirtAddr(*ppsMemDesc);
FWRTDataCpuMapError:
	RGXUnsetFirmwareAddress(*ppsMemDesc);
	DevmemFwFree(psDevInfo, *ppsMemDesc);
FWRTDataAllocateError:
	SyncPrimFree(psTmpCleanup->psCleanupSync);
SyncAlloc:
	*ppsCleanupData = NULL;
	OSFreeMem(psTmpCleanup);

AllocError:
	return eError;
}

/* Destroy HWRTDataSet */
PVRSRV_ERROR RGXDestroyHWRTData(RGX_RTDATA_CLEANUP_DATA *psCleanupData)
{
	PVRSRV_RGXDEV_INFO *psDevInfo;
	PVRSRV_ERROR eError;
	PRGXFWIF_HWRTDATA psHWRTData;
	IMG_UINT32 ui32Loop;

	PVR_ASSERT(psCleanupData);

	RGXSetFirmwareAddress(&psHWRTData, psCleanupData->psFWHWRTDataMemDesc, 0, RFW_FWADDR_NOREF_FLAG);

	/* Cleanup HWRTData in TA */
	eError = RGXFWRequestHWRTDataCleanUp(psCleanupData->psDeviceNode,
										 psHWRTData,
										 psCleanupData->psCleanupSync,
										 RGXFWIF_DM_TA);
	if (eError == PVRSRV_ERROR_RETRY)
	{
		return eError;
	}

	psDevInfo = psCleanupData->psDeviceNode->pvDevice;

	/* Cleanup HWRTData in 3D */
	eError = RGXFWRequestHWRTDataCleanUp(psCleanupData->psDeviceNode,
										 psHWRTData,
										 psCleanupData->psCleanupSync,
										 RGXFWIF_DM_3D);
	if (eError == PVRSRV_ERROR_RETRY)
	{
		return eError;
	}

	/* If we got here then TA and 3D operations on this RTData have finished */
	if (psCleanupData->psRTACtlMemDesc)
	{
		RGXUnsetFirmwareAddress(psCleanupData->psRTACtlMemDesc);
		DevmemFwFree(psDevInfo, psCleanupData->psRTACtlMemDesc);
	}

	RGXUnsetFirmwareAddress(psCleanupData->psFWHWRTDataMemDesc);
	DevmemFwFree(psDevInfo, psCleanupData->psFWHWRTDataMemDesc);

	if (psCleanupData->psRTArrayMemDesc)
	{
		RGXUnsetFirmwareAddress(psCleanupData->psRTArrayMemDesc);
		DevmemFwFree(psDevInfo, psCleanupData->psRTArrayMemDesc);
	}

	if (psCleanupData->psRendersAccArrayMemDesc)
	{
		RGXUnsetFirmwareAddress(psCleanupData->psRendersAccArrayMemDesc);
		DevmemFwFree(psDevInfo, psCleanupData->psRendersAccArrayMemDesc);
	}

	SyncPrimFree(psCleanupData->psCleanupSync);

	/* decrease freelist refcount */
	OSLockAcquire(psDevInfo->hLockFreeList);
	for (ui32Loop = 0; ui32Loop < RGXFW_MAX_FREELISTS; ui32Loop++)
	{
		PVR_ASSERT(psCleanupData->apsFreeLists[ui32Loop]->ui32RefCount > 0);
		psCleanupData->apsFreeLists[ui32Loop]->ui32RefCount--;
	}
	OSLockRelease(psDevInfo->hLockFreeList);

	OSFreeMem(psCleanupData);

	return PVRSRV_OK;
}

PVRSRV_ERROR RGXCreateFreeList(CONNECTION_DATA      *psConnection,
                               PVRSRV_DEVICE_NODE	*psDeviceNode,
							   IMG_UINT32			ui32MaxFLPages,
							   IMG_UINT32			ui32InitFLPages,
							   IMG_UINT32			ui32GrowFLPages,
                               IMG_UINT32           ui32GrowParamThreshold,
							   RGX_FREELIST			*psGlobalFreeList,
							   IMG_BOOL				bCheckFreelist,
							   IMG_DEV_VIRTADDR		sFreeListDevVAddr,
							   PMR					*psFreeListPMR,
							   IMG_DEVMEM_OFFSET_T	uiFreeListPMROffset,
							   RGX_FREELIST			**ppsFreeList)
{
	PVRSRV_ERROR				eError;
	RGXFWIF_FREELIST			*psFWFreeList;
	DEVMEM_MEMDESC				*psFWFreelistMemDesc;
	RGX_FREELIST				*psFreeList;
	PVRSRV_RGXDEV_INFO			*psDevInfo = psDeviceNode->pvDevice;

	/* Allocate kernel freelist struct */
	psFreeList = OSAllocZMem(sizeof(*psFreeList));
	if (psFreeList == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXCreateFreeList: failed to allocate host data structure"));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorAllocHost;
	}

	/* Allocate cleanup sync */
	eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
						   &psFreeList->psCleanupSync,
						   "ta3d free list cleanup");
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXCreateFreeList: Failed to allocate cleanup sync (0x%x)",
				eError));
		goto SyncAlloc;
	}

	/*
	 * This FW FreeList context is only mapped into kernel for initialisation
	 * and reconstruction (at other times it is not mapped and only used by
	 * the FW. Therefore the GPU cache doesn't need coherency, and write-combine
	 * is suffice on the CPU side (WC buffer will be flushed at the first TA-kick)
	 */
	eError = DevmemFwAllocate(psDevInfo,
							sizeof(*psFWFreeList),
							PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
							PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
							PVRSRV_MEMALLOCFLAG_GPU_READABLE |
							PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
							PVRSRV_MEMALLOCFLAG_GPU_CACHE_INCOHERENT |
							PVRSRV_MEMALLOCFLAG_CPU_READABLE |
							PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
							PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
							PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE,
							"FwFreeList",
							&psFWFreelistMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXCreateFreeList: DevmemAllocate for RGXFWIF_FREELIST failed"));
		goto FWFreeListAlloc;
	}

	/* Initialise host data structures */
	psFreeList->psDevInfo = psDevInfo;
	psFreeList->psFreeListPMR = psFreeListPMR;
	psFreeList->uiFreeListPMROffset = uiFreeListPMROffset;
	psFreeList->psFWFreelistMemDesc = psFWFreelistMemDesc;
	RGXSetFirmwareAddress(&psFreeList->sFreeListFWDevVAddr, psFWFreelistMemDesc, 0, RFW_FWADDR_FLAG_NONE);
	psFreeList->ui32FreelistID = psDevInfo->ui32FreelistCurrID++;
	psFreeList->ui32FreelistGlobalID = (psGlobalFreeList ? psGlobalFreeList->ui32FreelistID : 0);
	psFreeList->ui32MaxFLPages = ui32MaxFLPages;
	psFreeList->ui32InitFLPages = ui32InitFLPages;
	psFreeList->ui32GrowFLPages = ui32GrowFLPages;
	psFreeList->ui32CurrentFLPages = 0;
	psFreeList->ui32ReadyFLPages = 0;
	psFreeList->ui32GrowThreshold = ui32GrowParamThreshold;
	psFreeList->ui64FreelistChecksum = 0;
	psFreeList->ui32RefCount = 0;
	psFreeList->bCheckFreelist = bCheckFreelist;
	dllist_init(&psFreeList->sMemoryBlockHead);
	dllist_init(&psFreeList->sMemoryBlockInitHead);
	psFreeList->ownerPid = OSGetCurrentClientProcessIDKM();


	/* Add to list of freelists */
	OSLockAcquire(psDevInfo->hLockFreeList);
	dllist_add_to_tail(&psDevInfo->sFreeListHead, &psFreeList->sNode);
	OSLockRelease(psDevInfo->hLockFreeList);


	/* Initialise FW data structure */
	eError = DevmemAcquireCpuVirtAddr(psFreeList->psFWFreelistMemDesc, (void **)&psFWFreeList);
	PVR_LOGG_IF_ERROR(eError, "Devmem AcquireCpuVirtAddr", FWFreeListCpuMap);

	{
		const IMG_UINT32 ui32ReadyPages = _CalculateFreelistReadyPages(psFreeList, ui32InitFLPages);

		psFWFreeList->ui32MaxPages = ui32MaxFLPages;
		psFWFreeList->ui32CurrentPages = ui32InitFLPages - ui32ReadyPages;
		psFWFreeList->ui32GrowPages = ui32GrowFLPages;
		psFWFreeList->ui32CurrentStackTop = psFWFreeList->ui32CurrentPages - 1;
		psFWFreeList->psFreeListDevVAddr = sFreeListDevVAddr;
		psFWFreeList->ui64CurrentDevVAddr = (sFreeListDevVAddr.uiAddr +
			                                 ((ui32MaxFLPages - psFWFreeList->ui32CurrentPages) * sizeof(IMG_UINT32))) &
			                                ~((IMG_UINT64)RGX_BIF_PM_FREELIST_BASE_ADDR_ALIGNSIZE-1);
		psFWFreeList->ui32FreeListID = psFreeList->ui32FreelistID;
		psFWFreeList->bGrowPending = IMG_FALSE;
		psFWFreeList->ui32ReadyPages = ui32ReadyPages;
	}

	PVR_DPF((PVR_DBG_MESSAGE,"Freelist %p created: Max pages 0x%08x, Init pages 0x%08x, Max FL base address 0x%016" IMG_UINT64_FMTSPECx ", Init FL base address 0x%016" IMG_UINT64_FMTSPECx,
			psFreeList,
			ui32MaxFLPages,
			ui32InitFLPages,
			sFreeListDevVAddr.uiAddr,
			psFWFreeList->ui64CurrentDevVAddr));

	PDUMPCOMMENT("Dump FW FreeList");
	DevmemPDumpLoadMem(psFreeList->psFWFreelistMemDesc, 0, sizeof(*psFWFreeList), PDUMP_FLAGS_CONTINUOUS);

	/*
	 * Separate dump of the Freelist's number of Pages and stack pointer.
	 * This allows to easily modify the PB size in the out2.txt files.
	 */
	PDUMPCOMMENT("FreeList TotalPages");
	DevmemPDumpLoadMemValue32(psFreeList->psFWFreelistMemDesc,
							offsetof(RGXFWIF_FREELIST, ui32CurrentPages),
							psFWFreeList->ui32CurrentPages,
							PDUMP_FLAGS_CONTINUOUS);
	PDUMPCOMMENT("FreeList StackPointer");
	DevmemPDumpLoadMemValue32(psFreeList->psFWFreelistMemDesc,
							offsetof(RGXFWIF_FREELIST, ui32CurrentStackTop),
							psFWFreeList->ui32CurrentStackTop,
							PDUMP_FLAGS_CONTINUOUS);

	DevmemReleaseCpuVirtAddr(psFreeList->psFWFreelistMemDesc);


	/* Add initial PB block */
	eError = RGXGrowFreeList(psFreeList,
	                         ui32InitFLPages,
	                         &psFreeList->sMemoryBlockInitHead,
	                         IMG_TRUE);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"RGXCreateFreeList: failed to allocate initial memory block for free list 0x%016" IMG_UINT64_FMTSPECx " (error = %u)",
				sFreeListDevVAddr.uiAddr,
				eError));
		goto FWFreeListCpuMap;
	}
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
			/* Update Stats */
			PVRSRVStatsUpdateFreelistStats(1, /* Add 1 to the appropriate counter (Requests by App)*/
	                               0,
	                               psFreeList->ui32InitFLPages,
	                               psFreeList->ui32NumHighPages,
	                               psFreeList->ownerPid);

#endif

	/* return values */
	*ppsFreeList = psFreeList;

	return PVRSRV_OK;

	/* Error handling */

FWFreeListCpuMap:
	/* Remove freelists from list  */
	OSLockAcquire(psDevInfo->hLockFreeList);
	dllist_remove_node(&psFreeList->sNode);
	OSLockRelease(psDevInfo->hLockFreeList);

	RGXUnsetFirmwareAddress(psFWFreelistMemDesc);
	DevmemFwFree(psDevInfo, psFWFreelistMemDesc);

FWFreeListAlloc:
	SyncPrimFree(psFreeList->psCleanupSync);

SyncAlloc:
	OSFreeMem(psFreeList);

ErrorAllocHost:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}


/*
	RGXDestroyFreeList
*/
PVRSRV_ERROR RGXDestroyFreeList(RGX_FREELIST *psFreeList)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32RefCount;

	PVR_ASSERT(psFreeList);

	OSLockAcquire(psFreeList->psDevInfo->hLockFreeList);
	ui32RefCount = psFreeList->ui32RefCount;
	OSLockRelease(psFreeList->psDevInfo->hLockFreeList);

	if (ui32RefCount != 0)
	{
		/* Freelist still busy */
		return PVRSRV_ERROR_RETRY;
	}

	/* Freelist is not in use => start firmware cleanup */
	eError = RGXFWRequestFreeListCleanUp(psFreeList->psDevInfo,
										 psFreeList->sFreeListFWDevVAddr,
										 psFreeList->psCleanupSync);
	if (eError != PVRSRV_OK)
	{
		/* Can happen if the firmware took too long to handle the cleanup request,
		 * or if SLC-flushes didn't went through (due to some GPU lockup) */
		return eError;
	}

	/* Remove FreeList from linked list before we destroy it... */
	OSLockAcquire(psFreeList->psDevInfo->hLockFreeList);
	dllist_remove_node(&psFreeList->sNode);
	OSLockRelease(psFreeList->psDevInfo->hLockFreeList);

	if (psFreeList->bCheckFreelist)
	{
		RGXFWIF_FREELIST  *psFWFreeList;
		IMG_UINT64        ui32CurrentStackTop;
		IMG_UINT64        ui64CheckSum;

		/* Get the current stack pointer for this free list */
		DevmemAcquireCpuVirtAddr(psFreeList->psFWFreelistMemDesc, (void **)&psFWFreeList);
		ui32CurrentStackTop = psFWFreeList->ui32CurrentStackTop;
		DevmemReleaseCpuVirtAddr(psFreeList->psFWFreelistMemDesc);

		if (ui32CurrentStackTop == psFreeList->ui32CurrentFLPages-1)
		{
			/* Do consistency tests (as the list is fully populated) */
			_CheckFreelist(psFreeList, psFreeList->ui32CurrentFLPages + psFreeList->ui32ReadyFLPages, psFreeList->ui64FreelistChecksum, &ui64CheckSum);
		}
		else
		{
			/* Check for duplicate pages, but don't check the checksum as the list is not fully populated */
			_CheckFreelist(psFreeList, ui32CurrentStackTop+1, 0, &ui64CheckSum);
		}
	}

	/* Destroy FW structures */
	RGXUnsetFirmwareAddress(psFreeList->psFWFreelistMemDesc);
	DevmemFwFree(psFreeList->psDevInfo, psFreeList->psFWFreelistMemDesc);

	/* Remove grow shrink blocks */
	while (!dllist_is_empty(&psFreeList->sMemoryBlockHead))
	{
		eError = RGXShrinkFreeList(&psFreeList->sMemoryBlockHead, psFreeList);
		PVR_ASSERT(eError == PVRSRV_OK);
	}

	/* Remove initial PB block */
	eError = RGXShrinkFreeList(&psFreeList->sMemoryBlockInitHead, psFreeList);
	PVR_ASSERT(eError == PVRSRV_OK);

	/* consistency checks */
	PVR_ASSERT(dllist_is_empty(&psFreeList->sMemoryBlockInitHead));
	PVR_ASSERT(psFreeList->ui32CurrentFLPages == 0);

	SyncPrimFree(psFreeList->psCleanupSync);

	/* free Freelist */
	OSFreeMem(psFreeList);

	return eError;
}


/*
	RGXCreateRenderTarget
*/
PVRSRV_ERROR RGXCreateRenderTarget(CONNECTION_DATA      *psConnection,
                                   PVRSRV_DEVICE_NODE	*psDeviceNode,
								   IMG_DEV_VIRTADDR		psVHeapTableDevVAddr,
								   RGX_RT_CLEANUP_DATA 	**ppsCleanupData,
								   IMG_UINT32			*sRenderTargetFWDevVAddr)
{
	PVRSRV_ERROR			eError = PVRSRV_OK;
	RGXFWIF_RENDER_TARGET	*psRenderTarget;
	RGXFWIF_DEV_VIRTADDR	pFirmwareAddr;
	PVRSRV_RGXDEV_INFO 		*psDevInfo = psDeviceNode->pvDevice;
	RGX_RT_CLEANUP_DATA		*psCleanupData;

	PVR_UNREFERENCED_PARAMETER(psConnection);

	psCleanupData = OSAllocZMem(sizeof(*psCleanupData));
	if (psCleanupData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	psCleanupData->psDeviceNode = psDeviceNode;
	/*
	 * This FW render target context is only mapped into kernel for initialisation.
	 * Otherwise this allocation is only used by the FW.
	 * Therefore the GPU cache doesn't need coherency,
	 * and write-combine is suffice on the CPU side (WC buffer will be flushed at the first TA-kick)
	 */
	eError = DevmemFwAllocate(psDevInfo,
							sizeof(*psRenderTarget),
							PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
							PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
							PVRSRV_MEMALLOCFLAG_GPU_READABLE |
							PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
							PVRSRV_MEMALLOCFLAG_GPU_CACHE_INCOHERENT |
							PVRSRV_MEMALLOCFLAG_CPU_READABLE |
							PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
							PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
							PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE,
							"FwRenderTarget",
							&psCleanupData->psRenderTargetMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXCreateRenderTarget: DevmemAllocate for Render Target failed"));
		goto err_free;
	}
	RGXSetFirmwareAddress(&pFirmwareAddr, psCleanupData->psRenderTargetMemDesc, 0, RFW_FWADDR_FLAG_NONE);
	*sRenderTargetFWDevVAddr = pFirmwareAddr.ui32Addr;

	eError = DevmemAcquireCpuVirtAddr(psCleanupData->psRenderTargetMemDesc, (void **)&psRenderTarget);
	PVR_LOGG_IF_ERROR(eError, "Devmem AcquireCpuVirtAddr", err_fwalloc);

	psRenderTarget->psVHeapTableDevVAddr = psVHeapTableDevVAddr;
	psRenderTarget->bTACachesNeedZeroing = IMG_FALSE;
	PDUMPCOMMENT("Dump RenderTarget");
	DevmemPDumpLoadMem(psCleanupData->psRenderTargetMemDesc, 0, sizeof(*psRenderTarget), PDUMP_FLAGS_CONTINUOUS);
	DevmemReleaseCpuVirtAddr(psCleanupData->psRenderTargetMemDesc);

	*ppsCleanupData = psCleanupData;

err_out:
	return eError;

err_free:
	OSFreeMem(psCleanupData);
	goto err_out;

err_fwalloc:
	DevmemFwFree(psDevInfo, psCleanupData->psRenderTargetMemDesc);
	goto err_free;

}


/*
	RGXDestroyRenderTarget
*/
PVRSRV_ERROR RGXDestroyRenderTarget(RGX_RT_CLEANUP_DATA *psCleanupData)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = psCleanupData->psDeviceNode;

	RGXUnsetFirmwareAddress(psCleanupData->psRenderTargetMemDesc);

	/*
		Note:
		When we get RT cleanup in the FW call that instead
	*/
	/* Flush the SLC before freeing */
	{
		RGXFWIF_KCCB_CMD sFlushInvalCmd;
		PVRSRV_ERROR eError;

		/* Schedule the SLC flush command ... */
#if defined(PDUMP)
		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Submit SLC flush and invalidate");
#endif
		sFlushInvalCmd.eCmdType = RGXFWIF_KCCB_CMD_SLCFLUSHINVAL;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.bInval = IMG_TRUE;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.bDMContext = IMG_FALSE;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.eDM = 0;
		sFlushInvalCmd.uCmdData.sSLCFlushInvalData.psContext.ui32Addr = 0;

		eError = RGXSendCommandWithPowLock(psDeviceNode->pvDevice,
											RGXFWIF_DM_GP,
											&sFlushInvalCmd,
											sizeof(sFlushInvalCmd),
											PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXDestroyRenderTarget: Failed to schedule SLC flush command with error (%u)", eError));
		}
		else
		{
			/* Wait for the SLC flush to complete */
			eError = RGXWaitForFWOp(psDeviceNode->pvDevice, RGXFWIF_DM_GP, psDeviceNode->psSyncPrim, PDUMP_FLAGS_CONTINUOUS);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXDestroyRenderTarget: SLC flush and invalidate aborted with error (%u)", eError));
			}
		}
	}

	DevmemFwFree(psDeviceNode->pvDevice, psCleanupData->psRenderTargetMemDesc);
	OSFreeMem(psCleanupData);
	return PVRSRV_OK;
}

/*
	RGXCreateZSBuffer
*/
PVRSRV_ERROR RGXCreateZSBufferKM(CONNECTION_DATA * psConnection,
                                 PVRSRV_DEVICE_NODE	*psDeviceNode,
                                 DEVMEMINT_RESERVATION 	*psReservation,
                                 PMR 					*psPMR,
                                 PVRSRV_MEMALLOCFLAGS_T 	uiMapFlags,
                                 RGX_ZSBUFFER_DATA **ppsZSBuffer,
                                 IMG_UINT32 *pui32ZSBufferFWDevVAddr)
{
	PVRSRV_ERROR				eError;
	PVRSRV_RGXDEV_INFO 			*psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_PRBUFFER			*psFWZSBuffer;
	RGX_ZSBUFFER_DATA			*psZSBuffer;
	DEVMEM_MEMDESC				*psFWZSBufferMemDesc;
	IMG_BOOL					bOnDemand = PVRSRV_CHECK_ON_DEMAND(uiMapFlags) ? IMG_TRUE : IMG_FALSE;

	/* Allocate host data structure */
	psZSBuffer = OSAllocZMem(sizeof(*psZSBuffer));
	if (psZSBuffer == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXCreateZSBufferKM: Failed to allocate cleanup data structure for ZS-Buffer"));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorAllocCleanup;
	}

	eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
						   &psZSBuffer->psCleanupSync,
						   "ta3d zs buffer cleanup");
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXCreateZSBufferKM: Failed to allocate cleanup sync (0x%x)",
				eError));
		goto ErrorSyncAlloc;
	}

	/* Populate Host data */
	psZSBuffer->psDevInfo = psDevInfo;
	psZSBuffer->psReservation = psReservation;
	psZSBuffer->psPMR = psPMR;
	psZSBuffer->uiMapFlags = uiMapFlags;
	psZSBuffer->ui32RefCount = 0;
	psZSBuffer->bOnDemand = bOnDemand;
    if (bOnDemand)
    {
    	psZSBuffer->ui32ZSBufferID = psDevInfo->ui32ZSBufferCurrID++;
		psZSBuffer->psMapping = NULL;

		OSLockAcquire(psDevInfo->hLockZSBuffer);
    	dllist_add_to_tail(&psDevInfo->sZSBufferHead, &psZSBuffer->sNode);
		OSLockRelease(psDevInfo->hLockZSBuffer);
    }

	/* Allocate firmware memory for ZS-Buffer. */
	PDUMPCOMMENT("Allocate firmware ZS-Buffer data structure");
	eError = DevmemFwAllocate(psDevInfo,
							sizeof(*psFWZSBuffer),
							PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
							PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
							PVRSRV_MEMALLOCFLAG_GPU_READABLE |
							PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
							PVRSRV_MEMALLOCFLAG_GPU_CACHE_INCOHERENT |
							PVRSRV_MEMALLOCFLAG_CPU_READABLE |
							PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
							PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
							PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE,
							"FwZSBuffer",
							&psFWZSBufferMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXCreateZSBufferKM: Failed to allocate firmware ZS-Buffer (%u)", eError));
		goto ErrorAllocFWZSBuffer;
	}
	psZSBuffer->psFWZSBufferMemDesc = psFWZSBufferMemDesc;

	/* Temporarily map the firmware render context to the kernel. */
	eError = DevmemAcquireCpuVirtAddr(psFWZSBufferMemDesc,
                                      (void **)&psFWZSBuffer);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXCreateZSBufferKM: Failed to map firmware ZS-Buffer (%u)", eError));
		goto ErrorAcquireFWZSBuffer;
	}

	/* Populate FW ZS-Buffer data structure */
	psFWZSBuffer->bOnDemand = bOnDemand;
	psFWZSBuffer->eState = (bOnDemand) ? RGXFWIF_PRBUFFER_UNBACKED : RGXFWIF_PRBUFFER_BACKED;
	psFWZSBuffer->ui32BufferID = psZSBuffer->ui32ZSBufferID;

	/* Get firmware address of ZS-Buffer. */
	RGXSetFirmwareAddress(&psZSBuffer->sZSBufferFWDevVAddr, psFWZSBufferMemDesc, 0, RFW_FWADDR_FLAG_NONE);

	/* Dump the ZS-Buffer and the memory content */
	PDUMPCOMMENT("Dump firmware ZS-Buffer");
	DevmemPDumpLoadMem(psFWZSBufferMemDesc, 0, sizeof(*psFWZSBuffer), PDUMP_FLAGS_CONTINUOUS);

	/* Release address acquired above. */
	DevmemReleaseCpuVirtAddr(psFWZSBufferMemDesc);


	/* define return value */
	*ppsZSBuffer = psZSBuffer;
	*pui32ZSBufferFWDevVAddr = psZSBuffer->sZSBufferFWDevVAddr.ui32Addr;

	PVR_DPF((PVR_DBG_MESSAGE, "ZS-Buffer [%p] created (%s)",
							psZSBuffer,
							(bOnDemand) ? "On-Demand": "Up-front"));

	psZSBuffer->owner=OSGetCurrentClientProcessIDKM();

	return PVRSRV_OK;

	/* error handling */

ErrorAcquireFWZSBuffer:
	DevmemFwFree(psDevInfo, psFWZSBufferMemDesc);

ErrorAllocFWZSBuffer:
	SyncPrimFree(psZSBuffer->psCleanupSync);

ErrorSyncAlloc:
	OSFreeMem(psZSBuffer);

ErrorAllocCleanup:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}


/*
	RGXDestroyZSBuffer
*/
PVRSRV_ERROR RGXDestroyZSBufferKM(RGX_ZSBUFFER_DATA *psZSBuffer)
{
	POS_LOCK hLockZSBuffer;
	PVRSRV_ERROR eError;

	PVR_ASSERT(psZSBuffer);
	hLockZSBuffer = psZSBuffer->psDevInfo->hLockZSBuffer;

	/* Request ZS Buffer cleanup */
	eError = RGXFWRequestZSBufferCleanUp(psZSBuffer->psDevInfo,
										psZSBuffer->sZSBufferFWDevVAddr,
										psZSBuffer->psCleanupSync);
	if (eError != PVRSRV_ERROR_RETRY)
	{
		/* Free the firmware render context. */
    	RGXUnsetFirmwareAddress(psZSBuffer->psFWZSBufferMemDesc);
		DevmemFwFree(psZSBuffer->psDevInfo, psZSBuffer->psFWZSBufferMemDesc);

	    /* Remove Deferred Allocation from list */
		if (psZSBuffer->bOnDemand)
		{
			OSLockAcquire(hLockZSBuffer);
			PVR_ASSERT(dllist_node_is_in_list(&psZSBuffer->sNode));
			dllist_remove_node(&psZSBuffer->sNode);
			OSLockRelease(hLockZSBuffer);
		}

		SyncPrimFree(psZSBuffer->psCleanupSync);

		PVR_ASSERT(psZSBuffer->ui32RefCount == 0);

		PVR_DPF((PVR_DBG_MESSAGE,"ZS-Buffer [%p] destroyed",psZSBuffer));

		/* Free ZS-Buffer host data structure */
		OSFreeMem(psZSBuffer);

	}

	return eError;
}

PVRSRV_ERROR
RGXBackingZSBuffer(RGX_ZSBUFFER_DATA *psZSBuffer)
{
	POS_LOCK hLockZSBuffer;
	PVRSRV_ERROR eError;

	if (!psZSBuffer)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (!psZSBuffer->bOnDemand)
	{
		/* Only deferred allocations can be populated */
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_DPF((PVR_DBG_MESSAGE,"ZS Buffer [%p, ID=0x%08x]: Physical backing requested",
								psZSBuffer,
								psZSBuffer->ui32ZSBufferID));
	hLockZSBuffer = psZSBuffer->psDevInfo->hLockZSBuffer;

	OSLockAcquire(hLockZSBuffer);

	if (psZSBuffer->ui32RefCount == 0)
	{
		if (psZSBuffer->bOnDemand)
		{
			IMG_HANDLE hDevmemHeap;

			PVR_ASSERT(psZSBuffer->psMapping == NULL);

			/* Get Heap */
			eError = DevmemServerGetHeapHandle(psZSBuffer->psReservation, &hDevmemHeap);
			PVR_ASSERT(psZSBuffer->psMapping == NULL);

			eError = DevmemIntMapPMR(hDevmemHeap,
									psZSBuffer->psReservation,
									psZSBuffer->psPMR,
									psZSBuffer->uiMapFlags,
									&psZSBuffer->psMapping);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"Unable populate ZS Buffer [%p, ID=0x%08x] with error %u",
										psZSBuffer,
										psZSBuffer->ui32ZSBufferID,
										eError));
				OSLockRelease(hLockZSBuffer);
				return eError;

			}
			PVR_DPF((PVR_DBG_MESSAGE, "ZS Buffer [%p, ID=0x%08x]: Physical backing acquired",
										psZSBuffer,
										psZSBuffer->ui32ZSBufferID));
		}
	}

	/* Increase refcount*/
	psZSBuffer->ui32RefCount++;

	OSLockRelease(hLockZSBuffer);

	return PVRSRV_OK;
}


PVRSRV_ERROR
RGXPopulateZSBufferKM(RGX_ZSBUFFER_DATA *psZSBuffer,
					RGX_POPULATION **ppsPopulation)
{
	RGX_POPULATION *psPopulation;
	PVRSRV_ERROR eError;

	psZSBuffer->ui32NumReqByApp++;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	PVRSRVStatsUpdateZSBufferStats(1,0,psZSBuffer->owner);
#endif

	/* Do the backing */
	eError = RGXBackingZSBuffer(psZSBuffer);
	if (eError != PVRSRV_OK)
	{
		goto OnErrorBacking;
	}

	/* Create the handle to the backing */
	psPopulation = OSAllocMem(sizeof(*psPopulation));
	if (psPopulation == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto OnErrorAlloc;
	}

	psPopulation->psZSBuffer = psZSBuffer;

	/* return value */
	*ppsPopulation = psPopulation;

	return PVRSRV_OK;

OnErrorAlloc:
	RGXUnbackingZSBuffer(psZSBuffer);

OnErrorBacking:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

PVRSRV_ERROR
RGXUnbackingZSBuffer(RGX_ZSBUFFER_DATA *psZSBuffer)
{
	POS_LOCK hLockZSBuffer;
	PVRSRV_ERROR eError;

	if (!psZSBuffer)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_ASSERT(psZSBuffer->ui32RefCount);

	PVR_DPF((PVR_DBG_MESSAGE,"ZS Buffer [%p, ID=0x%08x]: Physical backing removal requested",
								psZSBuffer,
								psZSBuffer->ui32ZSBufferID));

	hLockZSBuffer = psZSBuffer->psDevInfo->hLockZSBuffer;

	OSLockAcquire(hLockZSBuffer);

	if (psZSBuffer->bOnDemand)
	{
		if (psZSBuffer->ui32RefCount == 1)
		{
			PVR_ASSERT(psZSBuffer->psMapping);

			eError = DevmemIntUnmapPMR(psZSBuffer->psMapping);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"Unable to unpopulate ZS Buffer [%p, ID=0x%08x] with error %u",
										psZSBuffer,
										psZSBuffer->ui32ZSBufferID,
										eError));
				OSLockRelease(hLockZSBuffer);
				return eError;
			}

			PVR_DPF((PVR_DBG_MESSAGE, "ZS Buffer [%p, ID=0x%08x]: Physical backing removed",
										psZSBuffer,
										psZSBuffer->ui32ZSBufferID));
		}
	}

	/* Decrease refcount*/
	psZSBuffer->ui32RefCount--;

	OSLockRelease(hLockZSBuffer);

	return PVRSRV_OK;
}

PVRSRV_ERROR
RGXUnpopulateZSBufferKM(RGX_POPULATION *psPopulation)
{
	PVRSRV_ERROR eError;

	if (!psPopulation)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eError = RGXUnbackingZSBuffer(psPopulation->psZSBuffer);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	OSFreeMem(psPopulation);

	return PVRSRV_OK;
}

static RGX_ZSBUFFER_DATA *FindZSBuffer(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_UINT32 ui32ZSBufferID)
{
	DLLIST_NODE *psNode, *psNext;
	RGX_ZSBUFFER_DATA *psZSBuffer = NULL;

	OSLockAcquire(psDevInfo->hLockZSBuffer);

	dllist_foreach_node(&psDevInfo->sZSBufferHead, psNode, psNext)
	{
		RGX_ZSBUFFER_DATA *psThisZSBuffer = IMG_CONTAINER_OF(psNode, RGX_ZSBUFFER_DATA, sNode);

		if (psThisZSBuffer->ui32ZSBufferID == ui32ZSBufferID)
		{
			psZSBuffer = psThisZSBuffer;
			break;
		}
	}

	OSLockRelease(psDevInfo->hLockZSBuffer);
	return psZSBuffer;
}

void RGXProcessRequestZSBufferBacking(PVRSRV_RGXDEV_INFO *psDevInfo,
									  IMG_UINT32 ui32ZSBufferID)
{
	RGX_ZSBUFFER_DATA *psZSBuffer;
	RGXFWIF_KCCB_CMD sTACCBCmd;
	PVRSRV_ERROR eError;

	PVR_ASSERT(psDevInfo);

	/* scan all deferred allocations */
	psZSBuffer = FindZSBuffer(psDevInfo, ui32ZSBufferID);

	if (psZSBuffer)
	{
		IMG_BOOL bBackingDone = IMG_TRUE;

		/* Populate ZLS */
		eError = RGXBackingZSBuffer(psZSBuffer);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"Populating ZS-Buffer failed with error %u (ID = 0x%08x)", eError, ui32ZSBufferID));
			bBackingDone = IMG_FALSE;
		}

		/* send confirmation */
		sTACCBCmd.eCmdType = RGXFWIF_KCCB_CMD_ZSBUFFER_BACKING_UPDATE;
		sTACCBCmd.uCmdData.sZSBufferBackingData.sZSBufferFWDevVAddr.ui32Addr = psZSBuffer->sZSBufferFWDevVAddr.ui32Addr;
		sTACCBCmd.uCmdData.sZSBufferBackingData.bDone = bBackingDone;

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError = RGXScheduleCommand(psDevInfo,
										RGXFWIF_DM_TA,
										&sTACCBCmd,
										sizeof(sTACCBCmd),
										0,
										PDUMP_FLAGS_NONE);
			if (eError != PVRSRV_ERROR_RETRY)
			{
				break;
			}
			OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();

		/* Kernel CCB should never fill up, as the FW is processing them right away  */
		PVR_ASSERT(eError == PVRSRV_OK);

		psZSBuffer->ui32NumReqByFW++;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
		PVRSRVStatsUpdateZSBufferStats(0,1,psZSBuffer->owner);
#endif

	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR,"ZS Buffer Lookup for ZS Buffer ID 0x%08x failed (Populate)", ui32ZSBufferID));
	}
}

void RGXProcessRequestZSBufferUnbacking(PVRSRV_RGXDEV_INFO *psDevInfo,
											IMG_UINT32 ui32ZSBufferID)
{
	RGX_ZSBUFFER_DATA *psZSBuffer;
	RGXFWIF_KCCB_CMD sTACCBCmd;
	PVRSRV_ERROR eError;

	PVR_ASSERT(psDevInfo);

	/* scan all deferred allocations */
	psZSBuffer = FindZSBuffer(psDevInfo, ui32ZSBufferID);

	if (psZSBuffer)
	{
		/* Unpopulate ZLS */
		eError = RGXUnbackingZSBuffer(psZSBuffer);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"UnPopulating ZS-Buffer failed failed with error %u (ID = 0x%08x)", eError, ui32ZSBufferID));
			PVR_ASSERT(IMG_FALSE);
		}

		/* send confirmation */
		sTACCBCmd.eCmdType = RGXFWIF_KCCB_CMD_ZSBUFFER_UNBACKING_UPDATE;
		sTACCBCmd.uCmdData.sZSBufferBackingData.sZSBufferFWDevVAddr.ui32Addr = psZSBuffer->sZSBufferFWDevVAddr.ui32Addr;
		sTACCBCmd.uCmdData.sZSBufferBackingData.bDone = IMG_TRUE;

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError = RGXScheduleCommand(psDevInfo,
										RGXFWIF_DM_TA,
										&sTACCBCmd,
										sizeof(sTACCBCmd),
										0,
										PDUMP_FLAGS_NONE);
			if (eError != PVRSRV_ERROR_RETRY)
			{
				break;
			}
			OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();

		/* Kernel CCB should never fill up, as the FW is processing them right away  */
		PVR_ASSERT(eError == PVRSRV_OK);

	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR,"ZS Buffer Lookup for ZS Buffer ID 0x%08x failed (UnPopulate)", ui32ZSBufferID));
	}
}

static
PVRSRV_ERROR _CreateTAContext(CONNECTION_DATA *psConnection,
							  PVRSRV_DEVICE_NODE *psDeviceNode,
							  DEVMEM_MEMDESC *psAllocatedMemDesc,
							  IMG_UINT32 ui32AllocatedOffset,
							  DEVMEM_MEMDESC *psFWMemContextMemDesc,
							  IMG_DEV_VIRTADDR sVDMCallStackAddr,
							  IMG_UINT32 ui32Priority,
							  RGX_COMMON_CONTEXT_INFO *psInfo,
							  RGX_SERVER_RC_TA_DATA *psTAData)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_TACTX_STATE *psContextState;
	PVRSRV_ERROR eError;
	/*
		Allocate device memory for the firmware GPU context suspend state.
		Note: the FW reads/writes the state to memory by accessing the GPU register interface.
	*/
	PDUMPCOMMENT("Allocate RGX firmware TA context suspend state");

	eError = DevmemFwAllocate(psDevInfo,
							  sizeof(RGXFWIF_TACTX_STATE),
							  RGX_FWCOMCTX_ALLOCFLAGS,
							  "FwTAContextState",
							  &psTAData->psContextStateMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateRenderContextKM: Failed to allocate firmware GPU context suspend state (%u)",
				eError));
		goto fail_tacontextsuspendalloc;
	}

	eError = DevmemAcquireCpuVirtAddr(psTAData->psContextStateMemDesc,
                                      (void **)&psContextState);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateRenderContextKM: Failed to map firmware render context state (%u)",
				eError));
		goto fail_suspendcpuvirtacquire;
	}
	psContextState->uTAReg_VDM_CALL_STACK_POINTER_Init = sVDMCallStackAddr.uiAddr;
	DevmemReleaseCpuVirtAddr(psTAData->psContextStateMemDesc);

	eError = FWCommonContextAllocate(psConnection,
									 psDeviceNode,
									 REQ_TYPE_TA,
									 RGXFWIF_DM_TA,
									 psAllocatedMemDesc,
									 ui32AllocatedOffset,
									 psFWMemContextMemDesc,
									 psTAData->psContextStateMemDesc,
									 RGX_TA_CCB_SIZE_LOG2,
									 ui32Priority,
									 psInfo,
									 &psTAData->psServerCommonContext);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateRenderContextKM: Failed to init TA fw common context (%u)",
				eError));
		goto fail_tacommoncontext;
	}

	/*
	 * Dump the FW 3D context suspend state buffer
	 */
	PDUMPCOMMENT("Dump the TA context suspend state buffer");
	DevmemPDumpLoadMem(psTAData->psContextStateMemDesc,
					   0,
					   sizeof(RGXFWIF_TACTX_STATE),
					   PDUMP_FLAGS_CONTINUOUS);

	psTAData->ui32Priority = ui32Priority;
	return PVRSRV_OK;

fail_tacommoncontext:
fail_suspendcpuvirtacquire:
	DevmemFwFree(psDevInfo, psTAData->psContextStateMemDesc);
fail_tacontextsuspendalloc:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

static
PVRSRV_ERROR _Create3DContext(CONNECTION_DATA *psConnection,
							  PVRSRV_DEVICE_NODE *psDeviceNode,
							  DEVMEM_MEMDESC *psAllocatedMemDesc,
							  IMG_UINT32 ui32AllocatedOffset,
							  DEVMEM_MEMDESC *psFWMemContextMemDesc,
							  IMG_UINT32 ui32Priority,
							  RGX_COMMON_CONTEXT_INFO *psInfo,
							  RGX_SERVER_RC_3D_DATA *ps3DData)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR eError;
	IMG_UINT	uiNumISPStoreRegs = 1; /* default value 1 expected */
	IMG_UINT	ui3DRegISPStateStoreSize = 0;

	/*
		Allocate device memory for the firmware GPU context suspend state.
		Note: the FW reads/writes the state to memory by accessing the GPU register interface.
	*/
	PDUMPCOMMENT("Allocate RGX firmware 3D context suspend state");

	if(!RGX_IS_FEATURE_SUPPORTED(psDevInfo, XE_MEMORY_HIERARCHY))
	{
		uiNumISPStoreRegs = psDeviceNode->pfnGetDeviceFeatureValue(psDeviceNode,
													RGX_FEATURE_NUM_ISP_IPP_PIPES_IDX);
	}

	/* Size of the CS buffer */
	/* Calculate the size of the 3DCTX ISP state */
	ui3DRegISPStateStoreSize = sizeof(RGXFWIF_3DCTX_STATE) +
			uiNumISPStoreRegs * sizeof(((RGXFWIF_3DCTX_STATE *)0)->au3DReg_ISP_STORE[0]);

	eError = DevmemFwAllocate(psDevInfo,
							  ui3DRegISPStateStoreSize,
							  RGX_FWCOMCTX_ALLOCFLAGS,
							  "Fw3DContextState",
							  &ps3DData->psContextStateMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateRenderContextKM: Failed to allocate firmware GPU context suspend state (%u)",
				eError));
		goto fail_3dcontextsuspendalloc;
	}

	eError = FWCommonContextAllocate(psConnection,
									 psDeviceNode,
									 REQ_TYPE_3D,
									 RGXFWIF_DM_3D,
									 psAllocatedMemDesc,
									 ui32AllocatedOffset,
									 psFWMemContextMemDesc,
									 ps3DData->psContextStateMemDesc,
									 RGX_3D_CCB_SIZE_LOG2,
									 ui32Priority,
									 psInfo,
									 &ps3DData->psServerCommonContext);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateRenderContextKM: Failed to init 3D fw common context (%u)",
				eError));
		goto fail_3dcommoncontext;
	}

	/*
	 * Dump the FW 3D context suspend state buffer
	 */
	PDUMPCOMMENT("Dump the 3D context suspend state buffer");
	DevmemPDumpLoadMem(ps3DData->psContextStateMemDesc,
					   0,
					   sizeof(RGXFWIF_3DCTX_STATE),
					   PDUMP_FLAGS_CONTINUOUS);

	ps3DData->ui32Priority = ui32Priority;
	return PVRSRV_OK;

fail_3dcommoncontext:
	DevmemFwFree(psDevInfo, ps3DData->psContextStateMemDesc);
fail_3dcontextsuspendalloc:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}


/*
 * PVRSRVRGXCreateRenderContextKM
 */
PVRSRV_ERROR PVRSRVRGXCreateRenderContextKM(CONNECTION_DATA				*psConnection,
											PVRSRV_DEVICE_NODE			*psDeviceNode,
											IMG_UINT32					ui32Priority,
											IMG_DEV_VIRTADDR			sVDMCallStackAddr,
											IMG_UINT32					ui32FrameworkRegisterSize,
											IMG_PBYTE					pabyFrameworkRegisters,
											IMG_HANDLE					hMemCtxPrivData,
											RGX_SERVER_RENDER_CONTEXT	**ppsRenderContext)
{
	PVRSRV_ERROR				eError;
	PVRSRV_RGXDEV_INFO 			*psDevInfo = psDeviceNode->pvDevice;
	RGX_SERVER_RENDER_CONTEXT	*psRenderContext;
	DEVMEM_MEMDESC				*psFWMemContextMemDesc = RGXGetFWMemDescFromMemoryContextHandle(hMemCtxPrivData);
	RGX_COMMON_CONTEXT_INFO		sInfo;

	/* Prepare cleanup structure */
	*ppsRenderContext = NULL;
	psRenderContext = OSAllocZMem(sizeof(*psRenderContext));
	if (psRenderContext == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	eError = OSLockCreate(&psRenderContext->hLock, LOCK_TYPE_NONE);


	if (eError != PVRSRV_OK)
	{
		goto fail_lock;
	}
#endif

	psRenderContext->psDeviceNode = psDeviceNode;

	/*
		Create the FW render context, this has the TA and 3D FW common
		contexts embedded within it
	*/
	eError = DevmemFwAllocate(psDevInfo,
							  sizeof(RGXFWIF_FWRENDERCONTEXT),
							  RGX_FWCOMCTX_ALLOCFLAGS,
							  "FwRenderContext",
							  &psRenderContext->psFWRenderContextMemDesc);
	if (eError != PVRSRV_OK)
	{
		goto fail_fwrendercontext;
	}

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	WorkEstInit(psDevInfo, &psRenderContext->sWorkEstData);
#endif

	/* Allocate cleanup sync */
	eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
						   &psRenderContext->psCleanupSync,
						   "ta3d render context cleanup");
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateRenderContextKM: Failed to allocate cleanup sync (0x%x)",
				eError));
		goto fail_syncalloc;
	}

	/*
	 * Create the FW framework buffer
	 */
	eError = PVRSRVRGXFrameworkCreateKM(psDeviceNode,
										&psRenderContext->psFWFrameworkMemDesc,
										ui32FrameworkRegisterSize);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateRenderContextKM: Failed to allocate firmware GPU framework state (%u)",
				eError));
		goto fail_frameworkcreate;
	}

	/* Copy the Framework client data into the framework buffer */
	eError = PVRSRVRGXFrameworkCopyCommand(psRenderContext->psFWFrameworkMemDesc,
										   pabyFrameworkRegisters,
										   ui32FrameworkRegisterSize);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateRenderContextKM: Failed to populate the framework buffer (%u)",
				eError));
		goto fail_frameworkcopy;
	}

	sInfo.psFWFrameworkMemDesc = psRenderContext->psFWFrameworkMemDesc;

	eError = _CreateTAContext(psConnection,
							  psDeviceNode,
							  psRenderContext->psFWRenderContextMemDesc,
							  offsetof(RGXFWIF_FWRENDERCONTEXT, sTAContext),
							  psFWMemContextMemDesc,
							  sVDMCallStackAddr,
							  ui32Priority,
							  &sInfo,
							  &psRenderContext->sTAData);
	if (eError != PVRSRV_OK)
	{
		goto fail_tacontext;
	}

	eError = _Create3DContext(psConnection,
							  psDeviceNode,
							  psRenderContext->psFWRenderContextMemDesc,
							  offsetof(RGXFWIF_FWRENDERCONTEXT, s3DContext),
							  psFWMemContextMemDesc,
							  ui32Priority,
							  &sInfo,
							  &psRenderContext->s3DData);
	if (eError != PVRSRV_OK)
	{
		goto fail_3dcontext;
	}

#if defined(SUPPORT_BUFFER_SYNC)
	psRenderContext->psBufferSyncContext =
		pvr_buffer_sync_context_create(psDeviceNode->psDevConfig->pvOSDevice,
									   "rogue-ta3d");
	if (IS_ERR(psRenderContext->psBufferSyncContext))
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: failed to create buffer_sync context (err=%ld)",
				 __func__, PTR_ERR(psRenderContext->psBufferSyncContext)));

		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto fail_buffer_sync_context_create;
	}
#endif

	SyncAddrListInit(&psRenderContext->sSyncAddrListTAFence);
	SyncAddrListInit(&psRenderContext->sSyncAddrListTAUpdate);
	SyncAddrListInit(&psRenderContext->sSyncAddrList3DFence);
	SyncAddrListInit(&psRenderContext->sSyncAddrList3DUpdate);

	{
		PVRSRV_RGXDEV_INFO			*psDevInfo = psDeviceNode->pvDevice;

		OSWRLockAcquireWrite(psDevInfo->hRenderCtxListLock);
		dllist_add_to_tail(&(psDevInfo->sRenderCtxtListHead), &(psRenderContext->sListNode));
		OSWRLockReleaseWrite(psDevInfo->hRenderCtxListLock);
	}

	*ppsRenderContext= psRenderContext;
	return PVRSRV_OK;

#if defined(SUPPORT_BUFFER_SYNC)
fail_buffer_sync_context_create:
	_Destroy3DContext(&psRenderContext->s3DData,
					  psRenderContext->psDeviceNode,
					  psRenderContext->psCleanupSync);
#endif
fail_3dcontext:
	_DestroyTAContext(&psRenderContext->sTAData,
					  psDeviceNode,
					  psRenderContext->psCleanupSync);
fail_tacontext:
fail_frameworkcopy:
	DevmemFwFree(psDevInfo, psRenderContext->psFWFrameworkMemDesc);
fail_frameworkcreate:
	SyncPrimFree(psRenderContext->psCleanupSync);
fail_syncalloc:
	DevmemFwFree(psDevInfo, psRenderContext->psFWRenderContextMemDesc);
fail_fwrendercontext:
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroy(psRenderContext->hLock);
fail_lock:
#endif
	OSFreeMem(psRenderContext);
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

/*
 * PVRSRVRGXDestroyRenderContextKM
 */
PVRSRV_ERROR PVRSRVRGXDestroyRenderContextKM(RGX_SERVER_RENDER_CONTEXT *psRenderContext)
{
	PVRSRV_ERROR				eError;
	PVRSRV_RGXDEV_INFO 	*psDevInfo = psRenderContext->psDeviceNode->pvDevice;
	RGXFWIF_FWRENDERCONTEXT	*psFWRenderContext;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	IMG_UINT32 ui32WorkEstCCBSubmitted;
#endif

	/* remove node from list before calling destroy - as destroy, if successful
	 * will invalidate the node
	 * must be re-added if destroy fails
	 */
	OSWRLockAcquireWrite(psDevInfo->hRenderCtxListLock);
	dllist_remove_node(&(psRenderContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hRenderCtxListLock);

#if defined(SUPPORT_BUFFER_SYNC)
	pvr_buffer_sync_context_destroy(psRenderContext->psBufferSyncContext);
#endif

	/* Cleanup the TA if we haven't already */
	if ((psRenderContext->ui32CleanupStatus & RC_CLEANUP_TA_COMPLETE) == 0)
	{
		eError = _DestroyTAContext(&psRenderContext->sTAData,
								   psRenderContext->psDeviceNode,
								   psRenderContext->psCleanupSync);
		if (eError == PVRSRV_OK)
		{
			psRenderContext->ui32CleanupStatus |= RC_CLEANUP_TA_COMPLETE;
		}
		else
		{
			goto e0;
		}
	}

	/* Cleanup the 3D if we haven't already */
	if ((psRenderContext->ui32CleanupStatus & RC_CLEANUP_3D_COMPLETE) == 0)
	{
		eError = _Destroy3DContext(&psRenderContext->s3DData,
								   psRenderContext->psDeviceNode,
								   psRenderContext->psCleanupSync);
		if (eError == PVRSRV_OK)
		{
			psRenderContext->ui32CleanupStatus |= RC_CLEANUP_3D_COMPLETE;
		}
		else
		{
			goto e0;
		}
	}

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	eError = DevmemAcquireCpuVirtAddr(psRenderContext->psFWRenderContextMemDesc,
	                                  (void **)&psFWRenderContext);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"PVRSRVRGXDestroyRenderContextKM: Failed to map firmware render context (%u)",
		         eError));
		goto e0;
	}

	ui32WorkEstCCBSubmitted = psFWRenderContext->ui32WorkEstCCBSubmitted;

	DevmemReleaseCpuVirtAddr(psRenderContext->psFWRenderContextMemDesc);

	/* Check if all of the workload estimation CCB commands for this workload are read */
	if (ui32WorkEstCCBSubmitted != psRenderContext->sWorkEstData.ui32WorkEstCCBReceived)
	{
		eError = PVRSRV_ERROR_RETRY;
		goto e0;
	}
#endif

	/*
		Only if both TA and 3D contexts have been cleaned up can we
		free the shared resources
	*/
	if (psRenderContext->ui32CleanupStatus == (RC_CLEANUP_3D_COMPLETE | RC_CLEANUP_TA_COMPLETE))
	{

		/* Update SPM statistics */
		eError = DevmemAcquireCpuVirtAddr(psRenderContext->psFWRenderContextMemDesc,
	                                      (void **)&psFWRenderContext);
		if (eError == PVRSRV_OK)
		{
			DevmemReleaseCpuVirtAddr(psRenderContext->psFWRenderContextMemDesc);
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXDestroyRenderContextKM: Failed to map firmware render context (%u)",
					eError));
		}

		/* Free the framework buffer */
		DevmemFwFree(psDevInfo, psRenderContext->psFWFrameworkMemDesc);

		/* Free the firmware render context */
		DevmemFwFree(psDevInfo, psRenderContext->psFWRenderContextMemDesc);

		/* Free the cleanup sync */
		SyncPrimFree(psRenderContext->psCleanupSync);

		SyncAddrListDeinit(&psRenderContext->sSyncAddrListTAFence);
		SyncAddrListDeinit(&psRenderContext->sSyncAddrListTAUpdate);
		SyncAddrListDeinit(&psRenderContext->sSyncAddrList3DFence);
		SyncAddrListDeinit(&psRenderContext->sSyncAddrList3DUpdate);

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		WorkEstDeInit(psDevInfo, &psRenderContext->sWorkEstData);
#endif

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
		OSLockDestroy(psRenderContext->hLock);
#endif

		OSFreeMem(psRenderContext);
	}

	return PVRSRV_OK;

e0:
	OSWRLockAcquireWrite(psDevInfo->hRenderCtxListLock);
	dllist_add_to_tail(&(psDevInfo->sRenderCtxtListHead), &(psRenderContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hRenderCtxListLock);
	return eError;
}


/* TODO !!! this was local on the stack, and we managed to blow the stack for the kernel.
 * THIS - 46 argument function needs to be sorted out.
 */
#if defined(PVRSRV_USE_BRIDGE_LOCK)
static RGX_CCB_CMD_HELPER_DATA gasTACmdHelperData[CCB_CMD_HELPER_NUM_TA_COMMANDS];
static RGX_CCB_CMD_HELPER_DATA gas3DCmdHelperData[CCB_CMD_HELPER_NUM_3D_COMMANDS];
#endif

/*
 * PVRSRVRGXKickTA3DKM
 */
PVRSRV_ERROR PVRSRVRGXKickTA3DKM(RGX_SERVER_RENDER_CONTEXT	*psRenderContext,
								 IMG_UINT32					ui32ClientCacheOpSeqNum,
								 IMG_UINT32					ui32ClientTAFenceCount,
								 SYNC_PRIMITIVE_BLOCK		**apsClientTAFenceSyncPrimBlock,
								 IMG_UINT32					*paui32ClientTAFenceSyncOffset,
								 IMG_UINT32					*paui32ClientTAFenceValue,
								 IMG_UINT32					ui32ClientTAUpdateCount,
								 SYNC_PRIMITIVE_BLOCK		**apsClientTAUpdateSyncPrimBlock,
								 IMG_UINT32					*paui32ClientTAUpdateSyncOffset,
								 IMG_UINT32					*paui32ClientTAUpdateValue,
								 IMG_UINT32					ui32ServerTASyncPrims,
								 IMG_UINT32					*paui32ServerTASyncFlags,
								 SERVER_SYNC_PRIMITIVE 		**pasServerTASyncs,
								 IMG_UINT32					ui32Client3DFenceCount,
								 SYNC_PRIMITIVE_BLOCK		**apsClient3DFenceSyncPrimBlock,
								 IMG_UINT32					*paui32Client3DFenceSyncOffset,
								 IMG_UINT32					*paui32Client3DFenceValue,
								 IMG_UINT32					ui32Client3DUpdateCount,
								 SYNC_PRIMITIVE_BLOCK		**apsClient3DUpdateSyncPrimBlock,
								 IMG_UINT32					*paui32Client3DUpdateSyncOffset,
								 IMG_UINT32					*paui32Client3DUpdateValue,
								 IMG_UINT32					ui32Server3DSyncPrims,
								 IMG_UINT32					*paui32Server3DSyncFlags,
								 SERVER_SYNC_PRIMITIVE 		**pasServer3DSyncs,
								 SYNC_PRIMITIVE_BLOCK		*psPRFenceSyncPrimBlock,
								 IMG_UINT32					ui32PRFenceSyncOffset,
								 IMG_UINT32					ui32PRFenceValue,
#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
								 PVRSRV_FENCE				iCheckTAFence,
								 PVRSRV_TIMELINE			iUpdateTATimeline,
								 PVRSRV_FENCE				*piUpdateTAFence,
								 IMG_CHAR					szFenceNameTA[PVRSRV_SYNC_NAME_LENGTH],
#else
								 PVRSRV_FENCE				iCheckFence,
								 PVRSRV_TIMELINE			iUpdateTimeline,
								 PVRSRV_FENCE				*piUpdateFence,
								 IMG_CHAR					szFenceName[PVRSRV_SYNC_NAME_LENGTH],
#endif
								 PVRSRV_FENCE				iCheck3DFence,
								 PVRSRV_TIMELINE			iUpdate3DTimeline,
								 PVRSRV_FENCE				*piUpdate3DFence,
								 IMG_CHAR					szFenceName3D[PVRSRV_SYNC_NAME_LENGTH],
								 IMG_UINT32					ui32TACmdSize,
								 IMG_PBYTE					pui8TADMCmd,
								 IMG_UINT32					ui323DPRCmdSize,
								 IMG_PBYTE					pui83DPRDMCmd,
								 IMG_UINT32					ui323DCmdSize,
								 IMG_PBYTE					pui83DDMCmd,
								 IMG_UINT32					ui32ExtJobRef,
								 IMG_BOOL					bLastTAInScene,
								 IMG_BOOL					bKickTA,
								 IMG_BOOL					bKickPR,
								 IMG_BOOL					bKick3D,
								 IMG_BOOL					bAbort,
								 IMG_UINT32					ui32PDumpFlags,
								 RGX_RTDATA_CLEANUP_DATA	*psRTDataCleanup,
								 RGX_ZSBUFFER_DATA		*psZBuffer,
								 RGX_ZSBUFFER_DATA		*psSBuffer,
								 RGX_ZSBUFFER_DATA		*psMSAAScratchBuffer,
								 IMG_BOOL			bCommitRefCountsTA,
								 IMG_BOOL			bCommitRefCounts3D,
								 IMG_BOOL			*pbCommittedRefCountsTA,
								 IMG_BOOL			*pbCommittedRefCounts3D,
								 IMG_UINT32			ui32SyncPMRCount,
								 IMG_UINT32			*paui32SyncPMRFlags,
								 PMR				**ppsSyncPMRs,
								 IMG_UINT32			ui32RenderTargetSize,
								 IMG_UINT32			ui32NumberOfDrawCalls,
								 IMG_UINT32			ui32NumberOfIndices,
								 IMG_UINT32			ui32NumberOfMRTs,
								 IMG_UINT64			ui64DeadlineInus,
								 IMG_DEV_VIRTADDR	sRobustnessResetReason)
{
#if defined(PVRSRV_USE_BRIDGE_LOCK)
	/* if the bridge lock is present then we use the singular/global helper structures */
	RGX_CCB_CMD_HELPER_DATA *pasTACmdHelperData = gasTACmdHelperData;
	RGX_CCB_CMD_HELPER_DATA *pas3DCmdHelperData = gas3DCmdHelperData;
#else
	/* if there is no bridge lock then we use the per-context helper structures */
	RGX_CCB_CMD_HELPER_DATA *pasTACmdHelperData = psRenderContext->asTACmdHelperData;
	RGX_CCB_CMD_HELPER_DATA *pas3DCmdHelperData = psRenderContext->as3DCmdHelperData;
#endif

	IMG_UINT32				ui32TACmdCount=0;
	IMG_UINT32				ui323DCmdCount=0;
	IMG_UINT32				ui32TACmdOffset=0;
	IMG_UINT32				ui323DCmdOffset=0;
	RGXFWIF_UFO				sPRUFO;
	IMG_UINT32				i;
	PVRSRV_ERROR			eError = PVRSRV_OK;
	PVRSRV_ERROR			eError2;
#if !defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	PVRSRV_FENCE			iUpdateFence = PVRSRV_NO_FENCE;
#endif
	IMG_UINT32				ui32IntJobRef;
	IMG_BOOL                bCCBStateOpen = IMG_FALSE;

	IMG_UINT32				ui32ClientPRUpdateCount = 0;
	PRGXFWIF_UFO_ADDR		*pauiClientPRUpdateUFOAddress = NULL;
	IMG_UINT32				*paui32ClientPRUpdateValue = NULL;

	PRGXFWIF_TIMESTAMP_ADDR pPreAddr;
	PRGXFWIF_TIMESTAMP_ADDR pPostAddr;
	PRGXFWIF_UFO_ADDR       pRMWUFOAddr;

	PRGXFWIF_UFO_ADDR			*pauiClientTAFenceUFOAddress = NULL;
	PRGXFWIF_UFO_ADDR			*pauiClientTAUpdateUFOAddress = NULL;
	PRGXFWIF_UFO_ADDR			*pauiClient3DFenceUFOAddress = NULL;
	PRGXFWIF_UFO_ADDR			*pauiClient3DUpdateUFOAddress = NULL;
	PRGXFWIF_UFO_ADDR			uiPRFenceUFOAddress;

#if !defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	IMG_UINT64               uiCheckFenceUID = 0;
	IMG_UINT64               uiUpdateFenceUID = 0;
#else
	IMG_UINT64               uiCheckTAFenceUID = 0;
	IMG_UINT64               uiCheck3DFenceUID = 0;
	IMG_UINT64               uiUpdateTAFenceUID = 0;
	IMG_UINT64               uiUpdate3DFenceUID = 0;
#endif

#if defined (SUPPORT_FALLBACK_FENCE_SYNC) || defined (SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_BUFFER_SYNC)
	IMG_BOOL bTAFenceOnSyncCheckpointsOnly = IMG_FALSE;
#endif

#if defined (SUPPORT_NATIVE_FENCE_SYNC) || defined (SUPPORT_FALLBACK_FENCE_SYNC)
	IMG_BOOL b3DFenceOnSyncCheckpointsOnly = IMG_FALSE;
	#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	IMG_UINT32 ui32TAFenceTimelineUpdateValue = 0;
	IMG_UINT32 ui323DFenceTimelineUpdateValue = 0;

	/* 
	 * Count of the number of TA and 3D update values (may differ from number of
	 * TA and 3D updates later, as sync checkpoints do not need to specify a value)
	 */
	IMG_UINT32 ui32ClientPRUpdateValueCount = 0;
	IMG_UINT32 ui32ClientTAUpdateValueCount = ui32ClientTAUpdateCount;
	IMG_UINT32 ui32Client3DUpdateValueCount = ui32Client3DUpdateCount;
	PSYNC_CHECKPOINT *apsFenceTASyncCheckpoints = NULL;				/*!< TA fence checkpoints */
	PSYNC_CHECKPOINT *apsFence3DSyncCheckpoints = NULL;				/*!< 3D fence checkpoints */
	IMG_UINT32 ui32FenceTASyncCheckpointCount = 0;
	IMG_UINT32 ui32Fence3DSyncCheckpointCount = 0;
	PSYNC_CHECKPOINT psUpdateTASyncCheckpoint = NULL;				/*!< TA update checkpoint (output) */
	PSYNC_CHECKPOINT psUpdate3DSyncCheckpoint = NULL;				/*!< 3D update checkpoint (output) */
	PVRSRV_CLIENT_SYNC_PRIM *psTAFenceTimelineUpdateSync = NULL;
	PVRSRV_CLIENT_SYNC_PRIM *ps3DFenceTimelineUpdateSync = NULL;
	void *pvTAUpdateFenceFinaliseData = NULL;
	void *pv3DUpdateFenceFinaliseData = NULL;

	RGX_SYNC_DATA sTASyncData = {0};		/*!< Contains internal update syncs for TA */
	RGX_SYNC_DATA s3DSyncData = {0};		/*!< Contains internal update syncs for 3D */

	PVRSRV_FENCE	iUpdateTAFence = PVRSRV_NO_FENCE;
	PVRSRV_FENCE	iUpdate3DFence = PVRSRV_NO_FENCE;

	#else
	IMG_UINT32 ui32FenceTimelineUpdateValue = 0;
	/* 
	 * Count of the number of TA and 3D update values (may differ from number of
	 * TA and 3D updates later, as sync checkpoints do not need to specify a value)
	 */
	IMG_UINT32 ui32ClientPRUpdateValueCount = 0;
	IMG_UINT32 ui32Client3DUpdateValueCount = ui32Client3DUpdateCount;
	PSYNC_CHECKPOINT *apsFenceSyncCheckpoints = NULL;
	IMG_UINT32 ui32FenceSyncCheckpointCount = 0;
	PSYNC_CHECKPOINT psUpdateSyncCheckpoint = NULL;
	IMG_UINT32 *pui32IntAllocatedUpdateValues = NULL;
	PVRSRV_CLIENT_SYNC_PRIM *psFenceTimelineUpdateSync = NULL;
	void *pvUpdateFenceFinaliseData = NULL;

	#endif /* PVRSRV_SYNC_SEPARATE_TIMELINES */
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined (SUPPORT_FALLBACK_FENCE_SYNC) */
#if defined(SUPPORT_BUFFER_SYNC)
	PSYNC_CHECKPOINT *apsBufferFenceSyncCheckpoints = NULL;
	IMG_UINT32 ui32BufferFenceSyncCheckpointCount = 0;
	PSYNC_CHECKPOINT psBufferUpdateSyncCheckpoint = NULL;
#endif /* defined(SUPPORT_BUFFER_SYNC) */

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	RGXFWIF_WORKEST_KICK_DATA sWorkloadKickDataTA;
	RGXFWIF_WORKEST_KICK_DATA sWorkloadKickData3D;
	IMG_UINT32 ui32TACommandOffset = 0;
	IMG_UINT32 ui323DCommandOffset = 0;
	IMG_UINT32 ui32TACmdHeaderOffset = 0;
	IMG_UINT32 ui323DCmdHeaderOffset = 0;
	IMG_UINT32 ui323DFullRenderCommandOffset = 0;
	IMG_UINT32 ui32TACmdOffsetWrapCheck = 0;
	IMG_UINT32 ui323DCmdOffsetWrapCheck = 0;
#endif

#if defined(SUPPORT_BUFFER_SYNC)
	struct pvr_buffer_sync_append_data *psBufferSyncData = NULL;
#endif /* defined(SUPPORT_BUFFER_SYNC) */

#if !defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	PVR_UNREFERENCED_PARAMETER(iCheck3DFence);
	PVR_UNREFERENCED_PARAMETER(iUpdate3DTimeline);
	PVR_UNREFERENCED_PARAMETER(piUpdate3DFence);
	PVR_UNREFERENCED_PARAMETER(szFenceName3D);
#endif

#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
	if (iUpdateTATimeline >= 0 && !piUpdateTAFence)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	if (iUpdate3DTimeline >= 0 && !piUpdate3DFence)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#else /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */
	if (iUpdateTATimeline >= 0 || iUpdate3DTimeline >= 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Providing update timeline (TA=%d, 3D=%d) in non-supporting driver",
			__func__, iUpdateTATimeline, iUpdate3DTimeline));
	}
	if (iCheckFenceTA >= 0 || iCheckFence3D >= 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Providing check fence (TA=%d, 3D=%d) in non-supporting driver",
			__func__, iCheckFenceTA, iCheckFence3D));
	}
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */

	/* Ensure the string is null-terminated (Required for safety) */
	szFenceNameTA[PVRSRV_SYNC_NAME_LENGTH-1] = '\0';
	szFenceName3D[PVRSRV_SYNC_NAME_LENGTH-1] = '\0';
#else
#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
	if (iUpdateTimeline >= 0 && !piUpdateFence)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#else /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC) */
	if (iUpdateTimeline >= 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Providing update timeline (%d) in non-supporting driver",
			__func__, iUpdateTimeline));
	}
	if (iCheckFence >= 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Providing check fence (%d) in non-supporting driver",
			__func__, iCheckFence));
	}
#endif

	/* Ensure the string is null-terminated (Required for safety) */
	szFenceName[PVRSRV_SYNC_NAME_LENGTH-1] = '\0';
#endif /* PVRSRV_SYNC_SEPARATE_TIMELINES */

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	sWorkloadKickDataTA.ui64ReturnDataIndex = 0;
	sWorkloadKickDataTA.ui64CyclesPrediction = 0;
	sWorkloadKickData3D.ui64ReturnDataIndex = 0;
	sWorkloadKickData3D.ui64CyclesPrediction = 0;
#endif

	ui32IntJobRef = OSAtomicIncrement(&psRenderContext->hIntJobRef);

	CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32ClientTAFenceCount=%d, ui32ClientTAUpdateCount=%d, ui32Client3DFenceCount=%d, ui32Client3DUpdateCount=%d", __FUNCTION__,
	         ui32ClientTAFenceCount, ui32ClientTAUpdateCount, ui32Client3DFenceCount, ui32Client3DUpdateCount));
	CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32ServerTASyncPrims=%d, ui32Server3DSyncPrims=%d", __FUNCTION__, ui32ServerTASyncPrims, ui32Server3DSyncPrims));

	*pbCommittedRefCountsTA = IMG_FALSE;
	*pbCommittedRefCounts3D = IMG_FALSE;

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psRenderContext->hLock);
#endif

	CHKPT_DBG((PVR_DBG_ERROR, "%s: SyncAddrListPopulate(psRenderContext->sSyncAddrListTAFence, %d fences)...", __func__, ui32ClientTAFenceCount));
	eError = SyncAddrListPopulate(&psRenderContext->sSyncAddrListTAFence,
										ui32ClientTAFenceCount,
										apsClientTAFenceSyncPrimBlock,
										paui32ClientTAFenceSyncOffset);
	if (eError != PVRSRV_OK)
	{
		goto err_populate_sync_addr_list_ta_fence;
	}

	if (ui32ClientTAFenceCount)
	{
		pauiClientTAFenceUFOAddress = psRenderContext->sSyncAddrListTAFence.pasFWAddrs;
	}

	CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClientTAFenceUFOAddress=<%p> ", __func__, (void*)pauiClientTAFenceUFOAddress));

	CHKPT_DBG((PVR_DBG_ERROR, "%s: SyncAddrListPopulate(psRenderContext->sSyncAddrListTAUpdate, %d updates)...", __func__, ui32ClientTAUpdateCount));
	eError = SyncAddrListPopulate(&psRenderContext->sSyncAddrListTAUpdate,
										ui32ClientTAUpdateCount,
										apsClientTAUpdateSyncPrimBlock,
										paui32ClientTAUpdateSyncOffset);
	if (eError != PVRSRV_OK)
	{
		goto err_populate_sync_addr_list_ta_update;
	}

	if (ui32ClientTAUpdateCount)
	{
		pauiClientTAUpdateUFOAddress = psRenderContext->sSyncAddrListTAUpdate.pasFWAddrs;
	}
	CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClientTAUpdateUFOAddress=<%p> ", __func__, (void*)pauiClientTAUpdateUFOAddress));

	CHKPT_DBG((PVR_DBG_ERROR, "%s: SyncAddrListPopulate(psRenderContext->sSyncAddrList3DFence, %d fences)...", __func__, ui32Client3DFenceCount));
	eError = SyncAddrListPopulate(&psRenderContext->sSyncAddrList3DFence,
										ui32Client3DFenceCount,
										apsClient3DFenceSyncPrimBlock,
										paui32Client3DFenceSyncOffset);
	if (eError != PVRSRV_OK)
	{
		goto err_populate_sync_addr_list_3d_fence;
	}

	if (ui32Client3DFenceCount)
	{
		pauiClient3DFenceUFOAddress = psRenderContext->sSyncAddrList3DFence.pasFWAddrs;
	}
	CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClient3DFenceUFOAddress=<%p> ", __func__, (void*)pauiClient3DFenceUFOAddress));

	CHKPT_DBG((PVR_DBG_ERROR, "%s: SyncAddrListPopulate(psRenderContext->sSyncAddrList3DUpdate, %d updates)...", __func__, ui32Client3DUpdateCount));
	eError = SyncAddrListPopulate(&psRenderContext->sSyncAddrList3DUpdate,
										ui32Client3DUpdateCount,
										apsClient3DUpdateSyncPrimBlock,
										paui32Client3DUpdateSyncOffset);
	if (eError != PVRSRV_OK)
	{
		goto err_populate_sync_addr_list_3d_update;
	}

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined(SUPPORT_FALLBACK_FENCE_SYNC)
	if (ui32Client3DUpdateCount || (iUpdate3DTimeline != PVRSRV_NO_TIMELINE && piUpdate3DFence && bKick3D))
#else
	if (ui32Client3DUpdateCount)
#endif
	{
		pauiClient3DUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
	}
	CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClient3DUpdateUFOAddress=<%p> ", __func__, (void*)pauiClient3DUpdateUFOAddress));

	eError = SyncPrimitiveBlockToFWAddr(psPRFenceSyncPrimBlock, ui32PRFenceSyncOffset, &uiPRFenceUFOAddress);

	if (eError != PVRSRV_OK)
	{
		goto err_pr_fence_address;
	}

#if (ENABLE_TA3D_UFO_DUMP == 1)
	{
		IMG_UINT32 ii;
		PRGXFWIF_UFO_ADDR *psTmpClientTAFenceUFOAddress = pauiClientTAFenceUFOAddress;
		IMG_UINT32 *pui32TmpClientTAFenceValue = paui32ClientTAFenceValue;
		PRGXFWIF_UFO_ADDR *psTmpClientTAUpdateUFOAddress = pauiClientTAUpdateUFOAddress;
		IMG_UINT32 *pui32TmpClientTAUpdateValue = paui32ClientTAUpdateValue;
		PRGXFWIF_UFO_ADDR *psTmpClient3DFenceUFOAddress = pauiClient3DFenceUFOAddress;
		IMG_UINT32 *pui32TmpClient3DFenceValue = paui32Client3DFenceValue;
		PRGXFWIF_UFO_ADDR *psTmpClient3DUpdateUFOAddress = pauiClient3DUpdateUFOAddress;
		IMG_UINT32 *pui32TmpClient3DUpdateValue = paui32Client3DUpdateValue;

		PVR_DPF((PVR_DBG_ERROR, "%s: ~~~ After populating sync prims ~~~", __FUNCTION__));

		/* Dump Fence syncs, Update syncs and PR Update syncs */
		PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d TA fence syncs:", __FUNCTION__, ui32ClientTAFenceCount));
		for (ii=0; ii<ui32ClientTAFenceCount; ii++)
		{
			if (psTmpClientTAFenceUFOAddress->ui32Addr & 0x1)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32ClientTAFenceCount, (void*)psTmpClientTAFenceUFOAddress, psTmpClientTAFenceUFOAddress->ui32Addr));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=%d(0x%x)", __FUNCTION__, ii+1, ui32ClientTAFenceCount, (void*)psTmpClientTAFenceUFOAddress, psTmpClientTAFenceUFOAddress->ui32Addr, *pui32TmpClientTAFenceValue, *pui32TmpClientTAFenceValue));
				pui32TmpClientTAFenceValue++;
			}
			psTmpClientTAFenceUFOAddress++;
		}
		PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d TA update syncs:", __FUNCTION__, ui32ClientTAUpdateCount));
		for (ii=0; ii<ui32ClientTAUpdateCount; ii++)
		{
			if (psTmpClientTAUpdateUFOAddress->ui32Addr & 0x1)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32ClientTAUpdateCount, (void*)psTmpClientTAUpdateUFOAddress, psTmpClientTAUpdateUFOAddress->ui32Addr));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=%d", __FUNCTION__, ii+1, ui32ClientTAUpdateCount, (void*)psTmpClientTAUpdateUFOAddress, psTmpClientTAUpdateUFOAddress->ui32Addr, *pui32TmpClientTAUpdateValue));
				pui32TmpClientTAUpdateValue++;
			}
			psTmpClientTAUpdateUFOAddress++;
		}
		PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d 3D fence syncs:", __FUNCTION__, ui32Client3DFenceCount));
		for (ii=0; ii<ui32Client3DFenceCount; ii++)
		{
			if (psTmpClient3DFenceUFOAddress->ui32Addr & 0x1)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32Client3DFenceCount, (void*)psTmpClient3DFenceUFOAddress, psTmpClient3DFenceUFOAddress->ui32Addr));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=%d", __FUNCTION__, ii+1, ui32Client3DFenceCount, (void*)psTmpClient3DFenceUFOAddress, psTmpClient3DFenceUFOAddress->ui32Addr, *pui32TmpClient3DFenceValue));
				pui32TmpClient3DFenceValue++;
			}
			psTmpClient3DFenceUFOAddress++;
		}
		PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d 3D update syncs:", __FUNCTION__, ui32Client3DUpdateCount));
		for (ii=0; ii<ui32Client3DUpdateCount; ii++)
		{
			if (psTmpClient3DUpdateUFOAddress->ui32Addr & 0x1)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32Client3DUpdateCount, (void*)psTmpClient3DUpdateUFOAddress, psTmpClient3DUpdateUFOAddress->ui32Addr));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=%d", __FUNCTION__, ii+1, ui32Client3DUpdateCount, (void*)psTmpClient3DUpdateUFOAddress, psTmpClient3DUpdateUFOAddress->ui32Addr, *pui32TmpClient3DUpdateValue));
				pui32TmpClient3DUpdateValue++;
			}
			psTmpClient3DUpdateUFOAddress++;
		}
	}
#endif

	/* Sanity check the server fences */
	for (i=0;i<ui32ServerTASyncPrims;i++)
	{
		if (!(paui32ServerTASyncFlags[i] & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Server fence (on TA) must fence", __FUNCTION__));
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
			OSLockRelease(psRenderContext->hLock);
#endif
			return PVRSRV_ERROR_INVALID_SYNC_PRIM_OP;
		}
	}

	for (i=0;i<ui32Server3DSyncPrims;i++)
	{
		if (!(paui32Server3DSyncFlags[i] & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Server fence (on 3D) must fence", __FUNCTION__));
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
			OSLockRelease(psRenderContext->hLock);
#endif
			return PVRSRV_ERROR_INVALID_SYNC_PRIM_OP;
		}
	}

	RGX_GetTimestampCmdHelper((PVRSRV_RGXDEV_INFO*) psRenderContext->psDeviceNode->pvDevice,
	                          & pPreAddr,
	                          & pPostAddr,
	                          & pRMWUFOAddr);

	/*
		Sanity check we have a PR kick if there are client or server fences
	*/
	if (!bKickPR && ((ui32Client3DFenceCount != 0) || (ui32Server3DSyncPrims != 0)))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: 3D fence (client or server) passed without a PR kick", __FUNCTION__));
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
		OSLockRelease(psRenderContext->hLock);
#endif
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32SyncPMRCount)
	{
#if defined(SUPPORT_BUFFER_SYNC)
		int err;

		if (!bKickTA)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Buffer sync only supported for kicks including a TA",
					 __FUNCTION__));
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
			OSLockRelease(psRenderContext->hLock);
#endif
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		if (!bKickPR)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Buffer sync only supported for kicks including a PR",
					 __FUNCTION__));
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
			OSLockRelease(psRenderContext->hLock);
#endif
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Calling pvr_buffer_sync_resolve_and_create_fences", __FUNCTION__));
		err = pvr_buffer_sync_resolve_and_create_fences(psRenderContext->psBufferSyncContext,
														ui32SyncPMRCount,
														ppsSyncPMRs,
														paui32SyncPMRFlags,
														&ui32BufferFenceSyncCheckpointCount,
														&apsBufferFenceSyncCheckpoints,
														&psBufferUpdateSyncCheckpoint,
														&psBufferSyncData);
		if (err)
		{
			eError = (err == -ENOMEM) ? PVRSRV_ERROR_OUT_OF_MEMORY : PVRSRV_ERROR_INVALID_PARAMS;
			PVR_DPF((PVR_DBG_ERROR, "%s:   pvr_buffer_sync_resolve_and_create_fences failed (%d)", __FUNCTION__, eError));
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
			OSLockRelease(psRenderContext->hLock);
#endif
			return eError;
		}

		/* Append buffer sync fences to TA fences */
		if (ui32BufferFenceSyncCheckpointCount > 0)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d buffer sync checkpoints to TA Fence (&psRenderContext->sSyncAddrListTAFence=<%p>, pauiClientTAFenceUFOAddress=<%p>)...", __FUNCTION__, ui32BufferFenceSyncCheckpointCount, (void*)&psRenderContext->sSyncAddrListTAFence , (void*)pauiClientTAFenceUFOAddress));
			SyncAddrListAppendAndDeRefCheckpoints(&psRenderContext->sSyncAddrListTAFence,
			                                      ui32BufferFenceSyncCheckpointCount,
			                                      apsBufferFenceSyncCheckpoints);
			if (!pauiClientTAFenceUFOAddress)
			{
				pauiClientTAFenceUFOAddress = psRenderContext->sSyncAddrListTAFence.pasFWAddrs;
			}
			if (ui32ClientTAFenceCount == 0)
			{
				bTAFenceOnSyncCheckpointsOnly = IMG_TRUE;
			}
			ui32ClientTAFenceCount += ui32BufferFenceSyncCheckpointCount;
		}

		if (psBufferUpdateSyncCheckpoint)
		{
			/* If we have a 3D kick append update to the 3D updates else append to the PR update */
			if (bKick3D)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 buffer sync checkpoint<%p> to 3D Update (&psRenderContext->sSyncAddrList3DUpdate=<%p>, pauiClient3DUpdateUFOAddress=<%p>)...", __FUNCTION__, (void*)psBufferUpdateSyncCheckpoint, (void*)&psRenderContext->sSyncAddrList3DUpdate , (void*)pauiClient3DUpdateUFOAddress));
				/* Append buffer sync update to 3D updates */
				SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrList3DUpdate,
											  1,
											  &psBufferUpdateSyncCheckpoint);
				if (!pauiClient3DUpdateUFOAddress)
				{
					pauiClient3DUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
				}
				ui32Client3DUpdateCount++;
			}
			else
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 buffer sync checkpoint<%p> to TA Update (&psRenderContext->sSyncAddrListTAUpdate=<%p>, pauiClient3DUpdateUFOAddress=<%p>)...", __FUNCTION__, (void*)psBufferUpdateSyncCheckpoint, (void*)&psRenderContext->sSyncAddrListTAUpdate , (void*)pauiClientTAUpdateUFOAddress));
				/* Append buffer sync update to TA updates */
				SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrListTAUpdate,
											  1,
											  &psBufferUpdateSyncCheckpoint);
				if (!pauiClientTAUpdateUFOAddress)
				{
					pauiClientTAUpdateUFOAddress = psRenderContext->sSyncAddrListTAUpdate.pasFWAddrs;
				}
				ui32ClientTAUpdateCount++;
			}
		}
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   (after buffer_sync) ui32ClientTAFenceCount=%d, ui32ClientTAUpdateCount=%d, ui32Client3DFenceCount=%d, ui32Client3DUpdateCount=%d, ui32ClientPRUpdateCount=%d,", __FUNCTION__, ui32ClientTAFenceCount, ui32ClientTAUpdateCount, ui32Client3DFenceCount, ui32Client3DUpdateCount, ui32ClientPRUpdateCount));

#else /* defined(SUPPORT_BUFFER_SYNC) */
		PVR_DPF((PVR_DBG_ERROR, "%s: Buffer sync not supported but got %u buffers", __FUNCTION__, ui32SyncPMRCount));
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
		OSLockRelease(psRenderContext->hLock);
#endif
		return PVRSRV_ERROR_INVALID_PARAMS;
#endif /* defined(SUPPORT_BUFFER_SYNC) */
	}

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined (SUPPORT_FALLBACK_FENCE_SYNC)

#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	if (iCheckTAFence >= 0 || iUpdateTATimeline >= 0 ||
		iCheck3DFence >= 0 || iUpdate3DTimeline >= 0)
	{
		PRGXFWIF_UFO_ADDR	*pauiClientTAIntUpdateUFOAddress = NULL;
		PRGXFWIF_UFO_ADDR	*pauiClient3DIntUpdateUFOAddress = NULL;

		CHKPT_DBG((PVR_DBG_ERROR, "%s: [TA] iCheckFence = %d, iUpdateTimeline = %d", __FUNCTION__, iCheckTAFence, iUpdateTATimeline));
		CHKPT_DBG((PVR_DBG_ERROR, "%s: [3D] iCheckFence = %d, iUpdateTimeline = %d", __FUNCTION__, iCheck3DFence, iUpdate3DTimeline));

		if (!bKickTA)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Native syncs only supported for kicks including a TA",
				__FUNCTION__));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto fail_fdsync;
		}
		if (!bKickPR)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Native syncs require a PR for all kicks",
				__FUNCTION__));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto fail_fdsync;
		}

		if (iCheckTAFence != PVRSRV_NO_FENCE)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointResolveFence[TA] (iCheckFence=%d), psRenderContext->psDeviceNode->hSyncCheckpointContext=<%p>...", __FUNCTION__, \
				iCheckTAFence, (void*)psRenderContext->psDeviceNode->hSyncCheckpointContext));
			/* Resolve the sync checkpoints that make up the input fence */
			eError = SyncCheckpointResolveFence(psRenderContext->psDeviceNode->hSyncCheckpointContext,
			                                    iCheckTAFence,
			                                    &ui32FenceTASyncCheckpointCount,
			                                    &apsFenceTASyncCheckpoints,
			                                    &uiCheckTAFenceUID);
			if (eError != PVRSRV_OK)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, returned ERROR (eError=%d)", __FUNCTION__, eError));
				goto fail_resolve_input_fence;
			}

			CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, fence %d contained %d checkpoints (apsFenceSyncCheckpoints=<%p>)", __FUNCTION__, \
				iCheckTAFence, ui32FenceTASyncCheckpointCount, (void*)apsFenceTASyncCheckpoints));
#if defined(TA3D_CHECKPOINT_DEBUG)
			if (apsFenceTASyncCheckpoints)
			{
				_DebugSyncCheckpoints(__FUNCTION__, "TA", apsFenceTASyncCheckpoints, ui32FenceTASyncCheckpointCount);
			}
#endif
		}

		if (iCheck3DFence != PVRSRV_NO_FENCE)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointResolveFence[3D] (iCheckFence=%d), psRenderContext->psDeviceNode->hSyncCheckpointContext=<%p>...", __FUNCTION__, \
				iCheck3DFence, (void*)psRenderContext->psDeviceNode->hSyncCheckpointContext));
			/* Resolve the sync checkpoints that make up the input fence */
			eError = SyncCheckpointResolveFence(psRenderContext->psDeviceNode->hSyncCheckpointContext,
			                                    iCheck3DFence,
			                                    &ui32Fence3DSyncCheckpointCount,
			                                    &apsFence3DSyncCheckpoints,
			                                    &uiCheck3DFenceUID);
			if (eError != PVRSRV_OK)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, returned ERROR (eError=%d)", __FUNCTION__, eError));
				goto fail_resolve_input_fence;
			}

			CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, fence %d contained %d checkpoints (apsFenceSyncCheckpoints=<%p>)", __FUNCTION__, \
				iCheck3DFence, ui32Fence3DSyncCheckpointCount, (void*)apsFence3DSyncCheckpoints));
#if defined(TA3D_CHECKPOINT_DEBUG)
			if (apsFence3DSyncCheckpoints)
			{
				_DebugSyncCheckpoints(__FUNCTION__, "3D", apsFence3DSyncCheckpoints, ui32Fence3DSyncCheckpointCount);
			}
#endif
		}

		{
			/* Create the output fence for TA (if required) */
			if (iUpdateTATimeline != PVRSRV_NO_TIMELINE)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointCreateFence[TA] (iUpdateFence=%d, iUpdateTimeline=%d,  psRenderContext->psDeviceNode->hSyncCheckpointContext=<%p>)", __FUNCTION__, \
					iUpdateTAFence, iUpdateTATimeline, (void*)psRenderContext->psDeviceNode->hSyncCheckpointContext));
				eError = SyncCheckpointCreateFence(psRenderContext->psDeviceNode,
				                                   szFenceNameTA,
				                                   iUpdateTATimeline,
				                                   psRenderContext->psDeviceNode->hSyncCheckpointContext,
				                                   &iUpdateTAFence,
				                                   &uiUpdateTAFenceUID,
				                                   &pvTAUpdateFenceFinaliseData,
				                                   &psUpdateTASyncCheckpoint,
				                                   (void*)&psTAFenceTimelineUpdateSync,
				                                   &ui32TAFenceTimelineUpdateValue);
				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   SyncCheckpointCreateFence[TA] failed (%d)", __FUNCTION__, eError));
					goto fail_create_output_fence;
				}

				CHKPT_DBG((PVR_DBG_ERROR, "%s: returned from SyncCheckpointCreateFence[TA] (iUpdateFence=%d, psFenceTimelineUpdateSync=<%p>, ui32FenceTimelineUpdateValue=0x%x)", __FUNCTION__, \
					iUpdateTAFence, (void*)psTAFenceTimelineUpdateSync, ui32TAFenceTimelineUpdateValue));

				/* Store the FW address of the update sync checkpoint in pauiClientTAIntUpdateUFOAddress */
				pauiClientTAIntUpdateUFOAddress = SyncCheckpointGetRGXFWIFUFOAddr(psUpdateTASyncCheckpoint);
				CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClientIntUpdateUFOAddress[TA]->ui32Addr=0x%x", __FUNCTION__, \
					pauiClientTAIntUpdateUFOAddress->ui32Addr));
			}

			/* Append the sync prim update for the TA timeline (if required) */
			if (psTAFenceTimelineUpdateSync)
			{
				sTASyncData.ui32ClientUpdateCount 		= ui32ClientTAUpdateCount;
				sTASyncData.ui32ClientUpdateValueCount	= ui32ClientTAUpdateValueCount;
				sTASyncData.ui32ClientPRUpdateValueCount= (bKick3D) ? 0 : ui32ClientPRUpdateValueCount;
				sTASyncData.paui32ClientUpdateValue		= paui32ClientTAUpdateValue;

				eError = RGXSyncAppendTimelineUpdate(ui32TAFenceTimelineUpdateValue,
											&psRenderContext->sSyncAddrListTAUpdate,
											(bKick3D) ? NULL : &psRenderContext->sSyncAddrList3DUpdate,
											psTAFenceTimelineUpdateSync,
											&sTASyncData,
											bKick3D);
				if (eError != PVRSRV_OK)
				{
					goto fail_alloc_update_values_mem_TA;
				}

				paui32ClientTAUpdateValue = sTASyncData.paui32ClientUpdateValue;
				ui32ClientTAUpdateValueCount = sTASyncData.ui32ClientUpdateValueCount;
				pauiClientTAUpdateUFOAddress = sTASyncData.pauiClientUpdateUFOAddress;
				ui32ClientTAUpdateCount = sTASyncData.ui32ClientUpdateCount;
			}

			/* Create the output fence for 3D (if required) */
			if (iUpdate3DTimeline != PVRSRV_NO_TIMELINE)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointCreateFence[3D] (iUpdateFence=%d, iUpdateTimeline=%d,  psRenderContext->psDeviceNode->hSyncCheckpointContext=<%p>)", __FUNCTION__, \
					iUpdate3DFence, iUpdate3DTimeline, (void*)psRenderContext->psDeviceNode->hSyncCheckpointContext));
				eError = SyncCheckpointCreateFence(psRenderContext->psDeviceNode,
				                                   szFenceName3D,
				                                   iUpdate3DTimeline,
				                                   psRenderContext->psDeviceNode->hSyncCheckpointContext,
				                                   &iUpdate3DFence,
				                                   &uiUpdate3DFenceUID,
				                                   &pv3DUpdateFenceFinaliseData,
				                                   &psUpdate3DSyncCheckpoint,
				                                   (void*)&ps3DFenceTimelineUpdateSync,
				                                   &ui323DFenceTimelineUpdateValue);
				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   SyncCheckpointCreateFence[3D] failed (%d)", __FUNCTION__, eError));
					goto fail_create_output_fence;
				}

				CHKPT_DBG((PVR_DBG_ERROR, "%s: returned from SyncCheckpointCreateFence[3D] (iUpdateFence=%d, psFenceTimelineUpdateSync=<%p>, ui32FenceTimelineUpdateValue=0x%x)", __FUNCTION__, \
					iUpdate3DFence, (void*)ps3DFenceTimelineUpdateSync, ui323DFenceTimelineUpdateValue));

				/* Store the FW address of the update sync checkpoint in pauiClient3DIntUpdateUFOAddress */
				pauiClient3DIntUpdateUFOAddress = SyncCheckpointGetRGXFWIFUFOAddr(psUpdate3DSyncCheckpoint);
				CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClientIntUpdateUFOAddress[3D]->ui32Addr=0x%x", __FUNCTION__, \
					pauiClient3DIntUpdateUFOAddress->ui32Addr));
			}

			/* Append the sync prim update for the 3D timeline (if required) */
			if (ps3DFenceTimelineUpdateSync)
			{
				s3DSyncData.ui32ClientUpdateCount = ui32Client3DUpdateCount;
				s3DSyncData.ui32ClientUpdateValueCount 	= ui32Client3DUpdateValueCount;
				s3DSyncData.ui32ClientPRUpdateValueCount= ui32ClientPRUpdateValueCount;
				s3DSyncData.paui32ClientUpdateValue = paui32Client3DUpdateValue;

				eError = RGXSyncAppendTimelineUpdate(ui323DFenceTimelineUpdateValue,
											&psRenderContext->sSyncAddrList3DUpdate,
											&psRenderContext->sSyncAddrList3DUpdate,	/*!< PR update: is this required? */
											ps3DFenceTimelineUpdateSync,
											&s3DSyncData,
											bKick3D);
				if (eError != PVRSRV_OK)
				{
					goto fail_alloc_update_values_mem_3D;
				}

				/* FIXME: can this be optimised? */
				paui32Client3DUpdateValue = s3DSyncData.paui32ClientUpdateValue;
				ui32Client3DUpdateValueCount = s3DSyncData.ui32ClientUpdateValueCount;
				pauiClient3DUpdateUFOAddress = s3DSyncData.pauiClientUpdateUFOAddress;
				ui32Client3DUpdateCount = s3DSyncData.ui32ClientUpdateCount;

				if (!bKick3D)
				{
					paui32ClientPRUpdateValue = s3DSyncData.paui32ClientPRUpdateValue;
					ui32ClientPRUpdateValueCount = s3DSyncData.ui32ClientPRUpdateValueCount;
					pauiClientPRUpdateUFOAddress = s3DSyncData.pauiClientPRUpdateUFOAddress;
					ui32ClientPRUpdateCount = s3DSyncData.ui32ClientPRUpdateCount;
				}
			}

			/* If TA command present, attach TA synchronisation checks and updates */
			if (bKickTA)
			{

				/* Checks (from input fence) */
				if (ui32FenceTASyncCheckpointCount > 0)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to TA Fence (apsFenceSyncCheckpoints=<%p>)...", __FUNCTION__, \
						ui32FenceTASyncCheckpointCount, (void*)apsFenceTASyncCheckpoints));
					SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrListTAFence,
												  ui32FenceTASyncCheckpointCount,
												  apsFenceTASyncCheckpoints);
					if (!pauiClientTAFenceUFOAddress)
					{
						pauiClientTAFenceUFOAddress = psRenderContext->sSyncAddrListTAFence.pasFWAddrs;
					}
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32ClientTAFenceCount was %d, now %d}", __FUNCTION__, \
						ui32ClientTAFenceCount, ui32ClientTAFenceCount+ui32FenceTASyncCheckpointCount));
					if (ui32ClientTAFenceCount == 0)
					{
						bTAFenceOnSyncCheckpointsOnly = IMG_TRUE;
					}
					ui32ClientTAFenceCount += ui32FenceTASyncCheckpointCount;
				}
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32ClientTAFenceCount now %d}", __FUNCTION__, ui32ClientTAFenceCount));

				if (psUpdateTASyncCheckpoint)
				{
					/* Update (from output fence) */
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint<%p> (ID=%d) to TA Update...", __FUNCTION__, \
						(void*)psUpdateTASyncCheckpoint, SyncCheckpointGetId(psUpdateTASyncCheckpoint)));
					SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrListTAUpdate,
												  1,
												  &psUpdateTASyncCheckpoint);
					if (!pauiClientTAUpdateUFOAddress)
					{
						pauiClientTAUpdateUFOAddress = psRenderContext->sSyncAddrListTAUpdate.pasFWAddrs;
					}
					ui32ClientTAUpdateCount++;
				}

				if (!bKick3D && psUpdate3DSyncCheckpoint)
				{
					/* Attach update to the 3D (used for PR) Updates */
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint<%p> (ID=%d) to 3D(PR) Update...", __FUNCTION__, \
						(void*)psUpdate3DSyncCheckpoint, SyncCheckpointGetId(psUpdate3DSyncCheckpoint)));
					SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrList3DUpdate,
												  1,
												  &psUpdate3DSyncCheckpoint);
					if (!pauiClientPRUpdateUFOAddress)
					{
						pauiClientPRUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
					}
					ui32ClientPRUpdateCount++;
				}
			}

			if (bKick3D)
			{
				/* Attach checks and updates to the 3D */

				/* Checks (from input fence) */
				if (ui32Fence3DSyncCheckpointCount > 0)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to 3D Fence...", __FUNCTION__, ui32Fence3DSyncCheckpointCount));
					SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrList3DFence,
												  ui32Fence3DSyncCheckpointCount,
												  apsFence3DSyncCheckpoints);
					if (!pauiClient3DFenceUFOAddress)
					{
						pauiClient3DFenceUFOAddress = psRenderContext->sSyncAddrList3DFence.pasFWAddrs;
					}
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32Client3DFenceCount was %d, now %d}", __FUNCTION__, \
						ui32Client3DFenceCount, ui32Client3DFenceCount+ui32Fence3DSyncCheckpointCount));
					if (ui32Client3DFenceCount == 0)
					{
						b3DFenceOnSyncCheckpointsOnly = IMG_TRUE;
					}
					ui32Client3DFenceCount += ui32Fence3DSyncCheckpointCount;
				}
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32Client3DFenceCount was %d}", __FUNCTION__, ui32Client3DFenceCount));

				if (psUpdate3DSyncCheckpoint)
				{
					/* Update (from output fence) */
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint<%p> (ID=%d) to 3D Update...", __FUNCTION__, \
						(void*)psUpdate3DSyncCheckpoint, SyncCheckpointGetId(psUpdate3DSyncCheckpoint)));
					SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrList3DUpdate,
												  1,
												  &psUpdate3DSyncCheckpoint);
					if (!pauiClient3DUpdateUFOAddress)
					{
						pauiClient3DUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
					}
					ui32Client3DUpdateCount++;
				}
			}

			/* Search for foreign dependencies for non-GPU timelines */
			CHKPT_DBG((PVR_DBG_ERROR, "%s: Checking for foreign checkpoints (%d sync points)...", __FUNCTION__, \
				ui32Fence3DSyncCheckpointCount));
			for (i=0; i<ui32Fence3DSyncCheckpointCount; i++)
			{
				/* Use the foreign fence checkpoint type */
				if (  SyncCheckpointGetTimeline(apsFence3DSyncCheckpoints[i]) == SYNC_CHECKPOINT_FOREIGN_CHECKPOINT)
				{
					/* 3D Sync point represents foreign dependency, copy sync
					 * point to TA command fence. */
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint<%p> (ID=%d) to TA Fence...", __FUNCTION__, \
						(void*)apsFence3DSyncCheckpoints[i], SyncCheckpointGetId(apsFence3DSyncCheckpoints[i])));
					SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrListTAFence,
												  1,
												  &apsFence3DSyncCheckpoints[i]);
				}
			}

			CHKPT_DBG((PVR_DBG_ERROR, "%s:   (after pvr_sync) ui32ClientTAFenceCount=%d, ui32ClientTAUpdateCount=%d, ui32Client3DFenceCount=%d, ui32Client3DUpdateCount=%d, ui32ClientPRUpdateCount=%d,", __FUNCTION__, ui32ClientTAFenceCount, ui32ClientTAUpdateCount, ui32Client3DFenceCount, ui32Client3DUpdateCount, ui32ClientPRUpdateCount));
		}

		if (ui32ClientTAFenceCount)
		{
			PVR_ASSERT(pauiClientTAFenceUFOAddress);
			if (!bTAFenceOnSyncCheckpointsOnly)
			{
				PVR_ASSERT(paui32ClientTAFenceValue);
			}
		}
		if (ui32ClientTAUpdateCount)
		{
			PVR_ASSERT(pauiClientTAUpdateUFOAddress);
			if (ui32ClientTAUpdateValueCount>0)
				PVR_ASSERT(paui32ClientTAUpdateValue);
		}
		if (ui32Client3DFenceCount)
		{
			PVR_ASSERT(pauiClient3DFenceUFOAddress);
			if (!b3DFenceOnSyncCheckpointsOnly)
			{
				PVR_ASSERT(paui32Client3DFenceValue);
			}
		}
		if (ui32Client3DUpdateCount)
		{
			PVR_ASSERT(pauiClient3DUpdateUFOAddress);
			if (ui32Client3DUpdateValueCount>0)
				PVR_ASSERT(paui32Client3DUpdateValue);
		}
		if (ui32ClientPRUpdateCount)
		{
			PVR_ASSERT(pauiClientPRUpdateUFOAddress);
			if (ui32ClientPRUpdateValueCount>0)
				PVR_ASSERT(paui32ClientPRUpdateValue);
		}

	}
#else /* PVRSRV_SYNC_SEPARATE_TIMELINES */
	if (iCheckFence >= 0 || iUpdateTimeline >= 0)
	{
		PRGXFWIF_UFO_ADDR	*pauiClientIntUpdateUFOAddress = NULL;

		CHKPT_DBG((PVR_DBG_ERROR, "%s: iCheckFence = %d, iUpdateTimeline = %d", __FUNCTION__, iCheckFence, iUpdateTimeline));

		if (!bKickTA)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Native syncs only supported for kicks including a TA",
				__FUNCTION__));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto fail_fdsync;
		}
		if (!bKickPR)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Native syncs require a PR for all kicks",
				__FUNCTION__));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto fail_fdsync;
		}

		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointResolveFence (iCheckFence=%d), psRenderContext->psDeviceNode->hSyncCheckpointContext=<%p>...", __FUNCTION__,
				iCheckFence, (void*)psRenderContext->psDeviceNode->hSyncCheckpointContext));
			/* Resolve the sync checkpoints that make up the input fence */
			eError = SyncCheckpointResolveFence(psRenderContext->psDeviceNode->hSyncCheckpointContext,
			                                    iCheckFence,
			                                    &ui32FenceSyncCheckpointCount,
			                                    &apsFenceSyncCheckpoints,
			                                    &uiCheckFenceUID);
			if (eError != PVRSRV_OK)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, returned ERROR (eError=%d)", __FUNCTION__, eError));
				goto fail_resolve_input_fence;
			}
			CHKPT_DBG((PVR_DBG_ERROR, "%s: ...done, fence %d contained %d checkpoints (apsFenceSyncCheckpoints=<%p>)", __FUNCTION__,
				iCheckFence, ui32FenceSyncCheckpointCount, (void*)apsFenceSyncCheckpoints));
#if defined(TA3D_CHECKPOINT_DEBUG)
			if (apsFenceSyncCheckpoints)
			{
				_DebugSyncCheckpoints(__FUNCTION__, "", apsFenceSyncCheckpoints, ui32FenceSyncCheckpointCount);
			}
#endif
			/* Create the output fence (if required) */
			if (iUpdateTimeline != PVRSRV_NO_TIMELINE)
			{
				CHKPT_DBG((PVR_DBG_ERROR, "%s: calling SyncCheckpointCreateFence (iUpdateFence=%d, iUpdateTimeline=%d,  psRenderContext->psDeviceNode->hSyncCheckpointContext=<%p>)", __FUNCTION__, iUpdateFence, \
					iUpdateTimeline, (void*)psRenderContext->psDeviceNode->hSyncCheckpointContext));
				eError = SyncCheckpointCreateFence(psRenderContext->psDeviceNode,
				                                   szFenceName,
				                                   iUpdateTimeline,
				                                   psRenderContext->psDeviceNode->hSyncCheckpointContext,
				                                   &iUpdateFence,
				                                   &uiUpdateFenceUID,
				                                   &pvUpdateFenceFinaliseData,
				                                   &psUpdateSyncCheckpoint,
				                                   (void*)&psFenceTimelineUpdateSync,
				                                   &ui32FenceTimelineUpdateValue);
				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s:   SyncCheckpointCreateFence failed (%d)", __FUNCTION__, eError));
					goto fail_create_output_fence;
				}

				CHKPT_DBG((PVR_DBG_ERROR, "%s: returned from SyncCheckpointCreateFence (iUpdateFence=%d, psFenceTimelineUpdateSync=<%p>, ui32FenceTimelineUpdateValue=0x%x)", __FUNCTION__, \
					iUpdateFence, (void*)psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue));

				/* Store the FW address of the update sync checkpoint in pauiClientIntUpdateUFOAddress */
				pauiClientIntUpdateUFOAddress = SyncCheckpointGetRGXFWIFUFOAddr(psUpdateSyncCheckpoint);
				CHKPT_DBG((PVR_DBG_ERROR, "%s: pauiClientIntUpdateUFOAddress->ui32Addr=0x%x", __FUNCTION__, pauiClientIntUpdateUFOAddress->ui32Addr));
			}
			/* Append the sync prim update for the timeline (if required) */
			if (psFenceTimelineUpdateSync)
			{
				IMG_UINT32 *pui32TimelineUpdateWp = NULL;

				CHKPT_DBG((PVR_DBG_ERROR, "%s: About to allocate memory to hold updates in pui32IntAllocatedUpdateValues(currently <%p>)", __FUNCTION__,
					(void*)pui32IntAllocatedUpdateValues));
				if (bKick3D)
				{
					/* Allocate memory to hold the list of update values (including our timeline update) */
					pui32IntAllocatedUpdateValues = OSAllocMem(sizeof(*pui32IntAllocatedUpdateValues) * (ui32Client3DUpdateValueCount+1));
					if (!pui32IntAllocatedUpdateValues)
					{
						/* Failed to allocate memory */
						eError = PVRSRV_ERROR_OUT_OF_MEMORY;
						goto fail_alloc_update_values_mem;
					}
					OSCachedMemSet(pui32IntAllocatedUpdateValues, 0xcc, sizeof(*pui32IntAllocatedUpdateValues) * (ui32Client3DUpdateValueCount+1));
					CHKPT_DBG((PVR_DBG_ERROR, "%s: Copying %d 3D update values into pui32IntAllocatedUpdateValues(<%p>)", __FUNCTION__,
						ui32Client3DUpdateCount, (void*)pui32IntAllocatedUpdateValues));
					/* Copy the update values into the new memory, then append our timeline update value */
					OSCachedMemCopy(pui32IntAllocatedUpdateValues, paui32Client3DUpdateValue, ui32Client3DUpdateValueCount * sizeof(*paui32Client3DUpdateValue));
				}
				else
				{
					/* Allocate memory to hold our timeline update value (for PR update) */
					pui32IntAllocatedUpdateValues = OSAllocMem(sizeof(*pui32IntAllocatedUpdateValues) * (ui32ClientPRUpdateValueCount+1));
					if (!pui32IntAllocatedUpdateValues)
					{
						/* Failed to allocate memory */
						eError = PVRSRV_ERROR_OUT_OF_MEMORY;
						goto fail_alloc_update_values_mem;
					}
					OSCachedMemSet(pui32IntAllocatedUpdateValues, 0xcc, sizeof(*pui32IntAllocatedUpdateValues) * (ui32ClientPRUpdateValueCount+1));
				}
#if defined(TA3D_CHECKPOINT_DEBUG)
				if (bKick3D)
				{
					_DebugSyncValues(__FUNCTION__, pui32IntAllocatedUpdateValues, ui32Client3DUpdateValueCount);
				}
#endif
				/* Now set the additional update value and append the timeline sync prim addr to either the
				 * render context 3D (or TA) update list
				 */
				CHKPT_DBG((PVR_DBG_ERROR, "%s: Appending the additional update value (0x%x) to psRenderContext->sSyncAddrList%sUpdate...", __FUNCTION__, ui32FenceTimelineUpdateValue, bKick3D ? "3D" : "TA"));
				if (bKick3D)
				{
					pui32TimelineUpdateWp = pui32IntAllocatedUpdateValues + ui32Client3DUpdateValueCount;
					*pui32TimelineUpdateWp = ui32FenceTimelineUpdateValue;
					ui32Client3DUpdateValueCount++;
					ui32Client3DUpdateCount++;
					SyncAddrListAppendSyncPrim(&psRenderContext->sSyncAddrList3DUpdate,
												   psFenceTimelineUpdateSync);
					if (!pauiClient3DUpdateUFOAddress)
					{
						pauiClient3DUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
					}
					/* Update paui32Client3DUpdateValue to point to our new list of update values */
					paui32Client3DUpdateValue = pui32IntAllocatedUpdateValues;
				}
				else
				{
					/* Use the sSyncAddrList3DUpdate for PR (as it doesn't have one of its own) */
					pui32TimelineUpdateWp = pui32IntAllocatedUpdateValues;
					*pui32TimelineUpdateWp = ui32FenceTimelineUpdateValue;
					ui32ClientPRUpdateValueCount++;
					ui32ClientPRUpdateCount++;
					SyncAddrListAppendSyncPrim(&psRenderContext->sSyncAddrList3DUpdate,
											   psFenceTimelineUpdateSync);
					if (!pauiClientPRUpdateUFOAddress)
					{
						pauiClientPRUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
					}
					/* Update paui32ClientPRUpdateValue to point to our new list of update values */
					paui32ClientPRUpdateValue = pui32IntAllocatedUpdateValues;
				}
#if defined(TA3D_CHECKPOINT_DEBUG)
				{
					_DebugSyncValues(__FUNCTION__, pui32IntAllocatedUpdateValues, ui32Client3DUpdateValueCount);
				}
#endif
			}

			CHKPT_DBG((PVR_DBG_WARNING, "Attaching sync checkpoints: Kicking TA (%d), 3D (%d), %p", bKickTA, bKick3D, pauiClient3DUpdateUFOAddress));

			/* Attach 3D and TA synchronisation checks and updates as needed */
			if (bKick3D)
			{
				if (bKickTA)
				{
					/* we have a TA and 3D, attach checks to TA, updates to 3D */
					/* Checks (from input fence) */
					if (ui32FenceSyncCheckpointCount > 0)
					{
						CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to TA Fence (apsFenceSyncCheckpoints=<%p>)...", __FUNCTION__, ui32FenceSyncCheckpointCount, (void*)apsFenceSyncCheckpoints));
						SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrListTAFence,
													  ui32FenceSyncCheckpointCount,
													  apsFenceSyncCheckpoints);
						if (!pauiClientTAFenceUFOAddress)
						{
							pauiClientTAFenceUFOAddress = psRenderContext->sSyncAddrListTAFence.pasFWAddrs;
						}
						CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32ClientTAFenceCount was %d, now %d}", __FUNCTION__, ui32ClientTAFenceCount, ui32ClientTAFenceCount+ui32FenceSyncCheckpointCount));
						if (ui32ClientTAFenceCount == 0)
						{
							bTAFenceOnSyncCheckpointsOnly = IMG_TRUE;
						}
						ui32ClientTAFenceCount += ui32FenceSyncCheckpointCount;
					}
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32ClientTAFenceCount now %d}", __FUNCTION__, ui32ClientTAFenceCount));

					if (psUpdateSyncCheckpoint)
					{
						/* Update (from output fence) */
						CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint<%p> (ID=%d) to 3D Update...", __FUNCTION__, (void*)psUpdateSyncCheckpoint, SyncCheckpointGetId(psUpdateSyncCheckpoint)));
						SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrList3DUpdate,
													  1,
													  &psUpdateSyncCheckpoint);
						if (!pauiClient3DUpdateUFOAddress)
						{
							pauiClient3DUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
						}
						ui32Client3DUpdateCount++;
					}
				}
				else
				{
					/* we only have 3D, attach checks and updates to the 3D */
					/* Checks (from input fence) */
					if (ui32FenceSyncCheckpointCount > 0)
					{
						CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to 3D Fence...", __FUNCTION__, ui32FenceSyncCheckpointCount));
						SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrList3DFence,
													  ui32FenceSyncCheckpointCount,
													  apsFenceSyncCheckpoints);
						if (!pauiClient3DFenceUFOAddress)
						{
							pauiClient3DFenceUFOAddress = psRenderContext->sSyncAddrList3DFence.pasFWAddrs;
						}
						CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32Client3DFenceCount was %d, now %d}", __FUNCTION__, ui32Client3DFenceCount, ui32Client3DFenceCount+ui32FenceSyncCheckpointCount));
						if (ui32Client3DFenceCount == 0)
						{
							b3DFenceOnSyncCheckpointsOnly = IMG_TRUE;
						}
						ui32Client3DFenceCount += ui32FenceSyncCheckpointCount;
					}
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32Client3DFenceCount was %d}", __FUNCTION__, ui32Client3DFenceCount));

					if (psUpdateSyncCheckpoint)
					{
						/* Update (from output fence) */
						CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append 1 sync checkpoint<%p> (ID=%d) to 3D Update...", __FUNCTION__, (void*)psUpdateSyncCheckpoint, SyncCheckpointGetId(psUpdateSyncCheckpoint)));
						SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrList3DUpdate,
													  1,
													  &psUpdateSyncCheckpoint);
						if (!pauiClient3DUpdateUFOAddress)
						{
							pauiClient3DUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
						}
						ui32Client3DUpdateCount++;
					}
				}
			}
			else
			{
				/* we only have TA, attach checks to the TA */
				/* Checks (from input fence) */
				if (ui32FenceSyncCheckpointCount > 0)
				{
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   Append %d sync checkpoints to TA Fence...", __FUNCTION__, ui32FenceSyncCheckpointCount));
					SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrListTAFence,
												  ui32FenceSyncCheckpointCount,
												  apsFenceSyncCheckpoints);
					if (!pauiClientTAFenceUFOAddress)
					{
						pauiClientTAFenceUFOAddress = psRenderContext->sSyncAddrListTAFence.pasFWAddrs;
					}
					CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32ClientTAFenceCount was %d, now %d}", __FUNCTION__, ui32ClientTAFenceCount, ui32ClientTAFenceCount+ui32FenceSyncCheckpointCount));
					if (ui32ClientTAFenceCount == 0)
					{
						bTAFenceOnSyncCheckpointsOnly = IMG_TRUE;
					}
					ui32ClientTAFenceCount += ui32FenceSyncCheckpointCount;
				}
				CHKPT_DBG((PVR_DBG_ERROR, "%s:   {ui32ClientTAFenceCount now %d}", __FUNCTION__, ui32ClientTAFenceCount));

				if (psUpdateSyncCheckpoint)
				{
					/* Attach update to the 3D (used for PR) Updates */
					SyncAddrListAppendCheckpoints(&psRenderContext->sSyncAddrList3DUpdate,
												  1,
												  &psUpdateSyncCheckpoint);
					if (!pauiClientPRUpdateUFOAddress)
					{
						pauiClientPRUpdateUFOAddress = psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs;
					}
					ui32ClientPRUpdateCount++;
				}
			}
			CHKPT_DBG((PVR_DBG_ERROR, "%s:   (after pvr_sync) ui32ClientTAFenceCount=%d, ui32ClientTAUpdateCount=%d, ui32Client3DFenceCount=%d, ui32Client3DUpdateCount=%d, ui32ClientPRUpdateCount=%d,", __FUNCTION__, ui32ClientTAFenceCount, ui32ClientTAUpdateCount, ui32Client3DFenceCount, ui32Client3DUpdateCount, ui32ClientPRUpdateCount));
		}
		if (ui32ClientTAFenceCount)
		{
			PVR_ASSERT(pauiClientTAFenceUFOAddress);
			if (!bTAFenceOnSyncCheckpointsOnly)
			{
				PVR_ASSERT(paui32ClientTAFenceValue);
			}
		}
		if (ui32ClientTAUpdateCount)
		{
			PVR_ASSERT(pauiClientTAUpdateUFOAddress);
			/* We don't have TA updates from fences, so there should always be a value
			 * (fence updates are attached to the PR)
			 */
			PVR_ASSERT(paui32ClientTAUpdateValue);
		}
		if (ui32Client3DFenceCount)
		{
			PVR_ASSERT(pauiClient3DFenceUFOAddress);
			if (!b3DFenceOnSyncCheckpointsOnly)
			{
				PVR_ASSERT(paui32Client3DFenceValue);
			}
		}
		if (ui32Client3DUpdateCount)
		{
			PVR_ASSERT(pauiClient3DUpdateUFOAddress);
			if (ui32Client3DUpdateValueCount>0)
				PVR_ASSERT(paui32Client3DUpdateValue);
		}
		if (ui32ClientPRUpdateCount)
		{
			PVR_ASSERT(pauiClientPRUpdateUFOAddress);
			if (ui32ClientPRUpdateValueCount>0)
				PVR_ASSERT(paui32ClientPRUpdateValue);
		}
	}
#endif /* PVRSRV_SYNC_SEPARATE_TIMELINES */
#endif /* SUPPORT_NATIVE_FENCE_SYNC || defined (SUPPORT_FALLBACK_FENCE_SYNC) */

	CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32ClientTAFenceCount=%d, pauiClientTAFenceUFOAddress=<%p> Line ", __func__, ui32ClientTAFenceCount, (void*)paui32ClientTAFenceValue));
	CHKPT_DBG((PVR_DBG_ERROR, "%s: ui32ClientTAUpdateCount=%d, pauiClientTAUpdateUFOAddress=<%p> Line ", __func__, ui32ClientTAUpdateCount, (void*)pauiClientTAUpdateUFOAddress));
#if (ENABLE_TA3D_UFO_DUMP == 1)
	{
		IMG_UINT32 ii;
		PRGXFWIF_UFO_ADDR *psTmpClientTAFenceUFOAddress = pauiClientTAFenceUFOAddress;
		IMG_UINT32 *pui32TmpClientTAFenceValue = paui32ClientTAFenceValue;
		PRGXFWIF_UFO_ADDR *psTmpClientTAUpdateUFOAddress = pauiClientTAUpdateUFOAddress;
		IMG_UINT32 *pui32TmpClientTAUpdateValue = paui32ClientTAUpdateValue;
		PRGXFWIF_UFO_ADDR *psTmpClient3DFenceUFOAddress = pauiClient3DFenceUFOAddress;
		IMG_UINT32 *pui32TmpClient3DFenceValue = paui32Client3DFenceValue;
		PRGXFWIF_UFO_ADDR *psTmpClient3DUpdateUFOAddress = pauiClient3DUpdateUFOAddress;
		IMG_UINT32 *pui32TmpClient3DUpdateValue = paui32Client3DUpdateValue;

		PVR_DPF((PVR_DBG_ERROR, "%s: ~~~ After appending sync checkpoints ", __FUNCTION__));

		/* Dump Fence syncs, Update syncs and PR Update syncs */
		PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d TA fence syncs:", __FUNCTION__, ui32ClientTAFenceCount));
		for (ii=0; ii<ui32ClientTAFenceCount; ii++)
		{
			if (psTmpClientTAFenceUFOAddress->ui32Addr & 0x1)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32ClientTAFenceCount, (void*)psTmpClientTAFenceUFOAddress, psTmpClientTAFenceUFOAddress->ui32Addr));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=%d(0x%x)", __FUNCTION__, ii+1, ui32ClientTAFenceCount, (void*)psTmpClientTAFenceUFOAddress, psTmpClientTAFenceUFOAddress->ui32Addr, *pui32TmpClientTAFenceValue, *pui32TmpClientTAFenceValue));
				pui32TmpClientTAFenceValue++;
			}
			psTmpClientTAFenceUFOAddress++;
		}
		PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d TA update syncs:", __FUNCTION__, ui32ClientTAUpdateCount));
		for (ii=0; ii<ui32ClientTAUpdateCount; ii++)
		{
			if (psTmpClientTAUpdateUFOAddress->ui32Addr & 0x1)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32ClientTAUpdateCount, (void*)psTmpClientTAUpdateUFOAddress, psTmpClientTAUpdateUFOAddress->ui32Addr));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=%d(0x%x)", __FUNCTION__, ii+1, ui32ClientTAUpdateCount, (void*)psTmpClientTAUpdateUFOAddress, psTmpClientTAUpdateUFOAddress->ui32Addr, *pui32TmpClientTAUpdateValue, *pui32TmpClientTAUpdateValue));
				pui32TmpClientTAUpdateValue++;
			}
			psTmpClientTAUpdateUFOAddress++;
		}
		PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d 3D fence syncs:", __FUNCTION__, ui32Client3DFenceCount));
		for (ii=0; ii<ui32Client3DFenceCount; ii++)
		{
			if (psTmpClient3DFenceUFOAddress->ui32Addr & 0x1)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32Client3DFenceCount, (void*)psTmpClient3DFenceUFOAddress, psTmpClient3DFenceUFOAddress->ui32Addr));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, CheckValue=%d(0x%x)", __FUNCTION__, ii+1, ui32Client3DFenceCount, (void*)psTmpClient3DFenceUFOAddress, psTmpClient3DFenceUFOAddress->ui32Addr, *pui32TmpClient3DFenceValue, *pui32TmpClient3DFenceValue));
				pui32TmpClient3DFenceValue++;
			}
			psTmpClient3DFenceUFOAddress++;
		}
		PVR_DPF((PVR_DBG_ERROR, "%s: Prepared %d 3D update syncs:", __FUNCTION__, ui32Client3DUpdateCount));
		for (ii=0; ii<ui32Client3DUpdateCount; ii++)
		{
			if (psTmpClient3DUpdateUFOAddress->ui32Addr & 0x1)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=PVRSRV_SYNC_CHECKPOINT_SIGNALLED", __FUNCTION__, ii+1, ui32Client3DUpdateCount, (void*)psTmpClient3DUpdateUFOAddress, psTmpClient3DUpdateUFOAddress->ui32Addr));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "%s:   %d/%d<%p>. FWAddr=0x%x, UpdateValue=%d(0x%x)", __FUNCTION__, ii+1, ui32Client3DUpdateCount, (void*)psTmpClient3DUpdateUFOAddress, psTmpClient3DUpdateUFOAddress->ui32Addr, *pui32TmpClient3DUpdateValue, *pui32TmpClient3DUpdateValue));
				pui32TmpClient3DUpdateValue++;
			}
			psTmpClient3DUpdateUFOAddress++;
		}
	}
#endif

	/* Init and acquire to TA command if required */
	if (bKickTA)
	{
		RGX_SERVER_RC_TA_DATA *psTAData = &psRenderContext->sTAData;

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		/* Prepare workload estimation */
		WorkEstPrepare(psRenderContext->psDeviceNode->pvDevice,
		               &psRenderContext->sWorkEstData,
		               &psRenderContext->sWorkEstData.sWorkloadMatchingDataTA,
		               ui32RenderTargetSize,
		               ui32NumberOfDrawCalls,
		               ui32NumberOfIndices,
		               ui32NumberOfMRTs,
		               ui64DeadlineInus,
		               &sWorkloadKickDataTA);
#endif

		/* Init the TA command helper */
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   calling RGXCmdHelperInitCmdCCB(), ui32ClientTAFenceCount=%d, ui32ClientTAUpdateCount=%d", __FUNCTION__, ui32ClientTAFenceCount, ui32ClientTAUpdateCount));
		eError = RGXCmdHelperInitCmdCCB(FWCommonContextGetClientCCB(psTAData->psServerCommonContext),
		                                ui32ClientTAFenceCount,
		                                pauiClientTAFenceUFOAddress,
		                                paui32ClientTAFenceValue,
		                                ui32ClientTAUpdateCount,
		                                pauiClientTAUpdateUFOAddress,
		                                paui32ClientTAUpdateValue,
		                                ui32ServerTASyncPrims,
		                                paui32ServerTASyncFlags,
		                                SYNC_FLAG_MASK_ALL,
		                                pasServerTASyncs,
		                                ui32TACmdSize,
		                                pui8TADMCmd,
		                                & pPreAddr,
		                                (bKick3D ? NULL : & pPostAddr),
		                                (bKick3D ? NULL : & pRMWUFOAddr),
		                                RGXFWIF_CCB_CMD_TYPE_TA,
		                                ui32ExtJobRef,
		                                ui32IntJobRef,
		                                ui32PDumpFlags,
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		                                &sWorkloadKickDataTA,
#else
										NULL,
#endif
		                                "TA",
		                                bCCBStateOpen,
		                                pasTACmdHelperData,
										sRobustnessResetReason);
		if (eError != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: Failed, eError=%d, Line", __FUNCTION__, eError));
			goto fail_tacmdinit;
		}

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		/* The following is used to determine the offset of the command header containing
		   the workload estimation data so that can be accessed when the KCCB is read */
		ui32TACmdHeaderOffset = RGXCmdHelperGetDMCommandHeaderOffset(pasTACmdHelperData);
#endif

		eError = RGXCmdHelperAcquireCmdCCB(CCB_CMD_HELPER_NUM_TA_COMMANDS, pasTACmdHelperData);
		if (eError != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: Failed, eError=%d, Line", __FUNCTION__, eError));
			goto fail_taacquirecmd;
		}
		else
		{
			ui32TACmdCount++;
		}
	}

	/* Only kick the 3D if required */
	if (bKickPR)
	{
		RGX_SERVER_RC_3D_DATA *ps3DData = &psRenderContext->s3DData;

		/*
			The command helper doesn't know about the PR fence so create
			the command with all the fences against it and later create
			the PR command itself which _must_ come after the PR fence.
		*/
		sPRUFO.puiAddrUFO = uiPRFenceUFOAddress;
		sPRUFO.ui32Value = ui32PRFenceValue;

		/* Init the PR fence command helper */
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   calling RGXCmdHelperInitCmdCCB(), ui32Client3DFenceCount=%d", __FUNCTION__, ui32Client3DFenceCount));
		eError = RGXCmdHelperInitCmdCCB(FWCommonContextGetClientCCB(ps3DData->psServerCommonContext),
										ui32Client3DFenceCount,
										pauiClient3DFenceUFOAddress,
										paui32Client3DFenceValue,
										0,
										NULL,
										NULL,
										(bKick3D ? ui32Server3DSyncPrims : 0),
										paui32Server3DSyncFlags,
										PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK,
										pasServer3DSyncs,
										sizeof(sPRUFO),
										(IMG_UINT8*) &sPRUFO,
										NULL,
										NULL,
										NULL,
										RGXFWIF_CCB_CMD_TYPE_FENCE_PR,
										ui32ExtJobRef,
										ui32IntJobRef,
										ui32PDumpFlags,
										NULL,
										"3D-PR-Fence",
										bCCBStateOpen,
										&pas3DCmdHelperData[ui323DCmdCount++],
										sRobustnessResetReason);
		if (eError != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: Failed, eError=%d, Line", __FUNCTION__, eError));
			goto fail_prfencecmdinit;
		}

		/* Init the 3D PR command helper */
		/*
			Updates for Android (fence sync and Timeline sync prim) are provided in the PR-update
			if no 3D is present. This is so the timeline update cannot happen out of order with any
			other 3D already in flight for the same timeline (PR-updates are done in the 3D cCCB).
			This out of order timeline sync prim update could happen if we attach it to the TA update.
		*/
		if (ui32ClientPRUpdateCount)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: Line %d, ui32ClientPRUpdateCount=%d, pauiClientPRUpdateUFOAddress=0x%x, ui32ClientPRUpdateValueCount=%d, paui32ClientPRUpdateValue=0x%x", __FUNCTION__, __LINE__,
					 ui32ClientPRUpdateCount, pauiClientPRUpdateUFOAddress->ui32Addr, ui32ClientPRUpdateValueCount, (ui32ClientPRUpdateValueCount == 0) ? PVRSRV_SYNC_CHECKPOINT_SIGNALLED : *paui32ClientPRUpdateValue));
		}
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   calling RGXCmdHelperInitCmdCCB(), ui32ClientPRUpdateCount=%d", __FUNCTION__, ui32ClientPRUpdateCount));
		eError = RGXCmdHelperInitCmdCCB(FWCommonContextGetClientCCB(ps3DData->psServerCommonContext),
										0,
										NULL,
										NULL,
										ui32ClientPRUpdateCount,
										pauiClientPRUpdateUFOAddress,
										paui32ClientPRUpdateValue,
										0,
										NULL,
										SYNC_FLAG_MASK_ALL,
										NULL,
										ui323DPRCmdSize,
										pui83DPRDMCmd,
										NULL,
										NULL,
										NULL,
										RGXFWIF_CCB_CMD_TYPE_3D_PR,
										ui32ExtJobRef,
										ui32IntJobRef,
										ui32PDumpFlags,
										NULL,
										"3D-PR",
										bCCBStateOpen,
										&pas3DCmdHelperData[ui323DCmdCount++],
										sRobustnessResetReason);
		if (eError != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: Failed, eError=%d, Line", __FUNCTION__, eError));
			goto fail_prcmdinit;
		}
	}

	if (bKick3D || bAbort)
	{
		RGX_SERVER_RC_3D_DATA *ps3DData = &psRenderContext->s3DData;

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		/* Prepare workload estimation */
		WorkEstPrepare(psRenderContext->psDeviceNode->pvDevice,
		               &psRenderContext->sWorkEstData,
		               &psRenderContext->sWorkEstData.sWorkloadMatchingData3D,
		               ui32RenderTargetSize,
		               ui32NumberOfDrawCalls,
		               ui32NumberOfIndices,
		               ui32NumberOfMRTs,
		               ui64DeadlineInus,
		               &sWorkloadKickData3D);
#endif

		/* Init the 3D command helper */
		eError = RGXCmdHelperInitCmdCCB(FWCommonContextGetClientCCB(ps3DData->psServerCommonContext),
										0,
										NULL,
										NULL,
										ui32Client3DUpdateCount,
										pauiClient3DUpdateUFOAddress,
										paui32Client3DUpdateValue,
										ui32Server3DSyncPrims,
										paui32Server3DSyncFlags,
										PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE,
										pasServer3DSyncs,
										ui323DCmdSize,
										pui83DDMCmd,
										(bKickTA ? NULL : & pPreAddr),
										& pPostAddr,
										& pRMWUFOAddr,
										RGXFWIF_CCB_CMD_TYPE_3D,
										ui32ExtJobRef,
										ui32IntJobRef,
										ui32PDumpFlags,
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
										&sWorkloadKickData3D,
#else
										NULL,
#endif
										"3D",
										bCCBStateOpen,
										&pas3DCmdHelperData[ui323DCmdCount++],
										sRobustnessResetReason);
		if (eError != PVRSRV_OK)
		{
			CHKPT_DBG((PVR_DBG_ERROR, "%s: Failed, eError=%d, Line", __FUNCTION__, eError));
			goto fail_3dcmdinit;
		}

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		/* The following are used to determine the offset of the command header containing the workload estimation
		   data so that can be accessed when the KCCB is read */
		ui323DCmdHeaderOffset =	RGXCmdHelperGetDMCommandHeaderOffset(&pas3DCmdHelperData[ui323DCmdCount - 1]);
		ui323DFullRenderCommandOffset =	RGXCmdHelperGetCommandOffset(pas3DCmdHelperData, ui323DCmdCount - 1);
#endif
	}

	/* Protect against array overflow in RGXCmdHelperAcquireCmdCCB() */
	if (ui323DCmdCount > CCB_CMD_HELPER_NUM_3D_COMMANDS)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: Failed, eError=%d, Line", __FUNCTION__, eError));
		goto fail_3dcmdinit;
	}

	if (ui323DCmdCount)
	{
		PVR_ASSERT(bKickPR || bKick3D);

		/* Acquire space for all the 3D command(s) */
		eError = RGXCmdHelperAcquireCmdCCB(ui323DCmdCount, pas3DCmdHelperData);
		if (eError != PVRSRV_OK)
		{
			/* If RGXCmdHelperAcquireCmdCCB fails we skip the scheduling
			 * of a new TA command with the same Write offset in Kernel CCB.
			 */
			CHKPT_DBG((PVR_DBG_ERROR, "%s: Failed, eError=%d, Line", __FUNCTION__, eError));
			goto fail_3dacquirecmd;
		}
	}

	/*
		We should acquire the space in the kernel CCB here as after this point
		we release the commands which will take operations on server syncs
		which can't be undone
	*/

	/*
		Everything is ready to go now, release the commands
	*/
	if (ui32TACmdCount)
	{
		ui32TACmdOffset = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psRenderContext->sTAData.psServerCommonContext));
		RGXCmdHelperReleaseCmdCCB(ui32TACmdCount,
								  pasTACmdHelperData,
								  "TA",
								  FWCommonContextGetFWAddress(psRenderContext->sTAData.psServerCommonContext).ui32Addr);

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		ui32TACmdOffsetWrapCheck = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psRenderContext->sTAData.psServerCommonContext));

		/* This checks if the command would wrap around at the end of the CCB and therefore  would start at an
		   offset of 0 rather than the current command offset */
		if (ui32TACmdOffset < ui32TACmdOffsetWrapCheck)
		{
			ui32TACommandOffset = ui32TACmdOffset;
		}
		else
		{
			ui32TACommandOffset = 0;
		}
#endif
	}

	if (ui323DCmdCount)
	{
		ui323DCmdOffset = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psRenderContext->s3DData.psServerCommonContext));
		RGXCmdHelperReleaseCmdCCB(ui323DCmdCount,
								  pas3DCmdHelperData,
								  "3D",
								  FWCommonContextGetFWAddress(psRenderContext->s3DData.psServerCommonContext).ui32Addr);

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		ui323DCmdOffsetWrapCheck = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psRenderContext->s3DData.psServerCommonContext));

		if (ui323DCmdOffset < ui323DCmdOffsetWrapCheck)
		{
			ui323DCommandOffset = ui323DCmdOffset;
		}
		else
		{
			ui323DCommandOffset = 0;
		}
#endif
	}

	if (ui32TACmdCount)
	{
		RGXFWIF_KCCB_CMD sTAKCCBCmd;
		IMG_UINT32 ui32FWCtx = FWCommonContextGetFWAddress(psRenderContext->sTAData.psServerCommonContext).ui32Addr;

		/* Construct the kernel TA CCB command. */
		sTAKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
		sTAKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psRenderContext->sTAData.psServerCommonContext);
		sTAKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psRenderContext->sTAData.psServerCommonContext));

		/* Add the Workload data into the KCCB kick */
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		/* Store the offset to the CCCB command header so that it can be referenced when the KCCB command reaches the FW */
		sTAKCCBCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = ui32TACommandOffset + ui32TACmdHeaderOffset;
#else
		sTAKCCBCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = 0;
#endif

		if (bCommitRefCountsTA)
		{
			AttachKickResourcesCleanupCtls((PRGXFWIF_CLEANUP_CTL *) &sTAKCCBCmd.uCmdData.sCmdKickData.apsCleanupCtl,
										&sTAKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl,
										RGXFWIF_DM_TA,
										bKickTA,
										psRTDataCleanup,
										psZBuffer,
										psSBuffer,
										psMSAAScratchBuffer);
			*pbCommittedRefCountsTA = IMG_TRUE;
		}
		else
		{
			sTAKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;
		}

		HTBLOGK(HTB_SF_MAIN_KICK_TA,
				sTAKCCBCmd.uCmdData.sCmdKickData.psContext,
				ui32TACmdOffset
				);

#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
		RGX_HWPERF_HOST_ENQ(psRenderContext,
		                    OSGetCurrentClientProcessIDKM(),
		                    ui32FWCtx,
		                    ui32ExtJobRef,
		                    ui32IntJobRef,
		                    RGX_HWPERF_KICK_TYPE_TA,
							uiCheckTAFenceUID,
							uiUpdateTAFenceUID,
		                    ui64DeadlineInus,
		                    WORKEST_CYCLES_PREDICTION_GET(sWorkloadKickDataTA));
#else
		RGX_HWPERF_HOST_ENQ(psRenderContext,
		                    OSGetCurrentClientProcessIDKM(),
		                    ui32FWCtx,
		                    ui32ExtJobRef,
		                    ui32IntJobRef,
		                    RGX_HWPERF_KICK_TYPE_TA,
							uiCheckFenceUID,
							uiUpdateFenceUID,
		                    ui64DeadlineInus,
		                    WORKEST_CYCLES_PREDICTION_GET(sWorkloadKickDataTA));
#endif

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError2 = RGXScheduleCommand(psRenderContext->psDeviceNode->pvDevice,
										RGXFWIF_DM_TA,
										&sTAKCCBCmd,
										sizeof(sTAKCCBCmd),
										ui32ClientCacheOpSeqNum,
										ui32PDumpFlags);
			if (eError2 != PVRSRV_ERROR_RETRY)
			{
				break;
			}
			OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();

#if defined(SUPPORT_GPUTRACE_EVENTS)
		RGXHWPerfFTraceGPUEnqueueEvent(psRenderContext->psDeviceNode->pvDevice,
					ui32FWCtx, ui32IntJobRef, RGX_HWPERF_KICK_TYPE_TA3D);
#endif
	}

	if (ui323DCmdCount)
	{
		RGXFWIF_KCCB_CMD s3DKCCBCmd;
		IMG_UINT32 ui32FWCtx = FWCommonContextGetFWAddress(psRenderContext->s3DData.psServerCommonContext).ui32Addr;

		/* Construct the kernel 3D CCB command. */
		s3DKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
		s3DKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psRenderContext->s3DData.psServerCommonContext);
		s3DKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psRenderContext->s3DData.psServerCommonContext));

		/* Add the Workload data into the KCCB kick */
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
		/* Store the offset to the CCCB command header so that it can be referenced when the KCCB command reaches the FW */
		s3DKCCBCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = ui323DCommandOffset + ui323DCmdHeaderOffset + ui323DFullRenderCommandOffset;
#else
		s3DKCCBCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = 0;
#endif

		if (bCommitRefCounts3D)
		{
			AttachKickResourcesCleanupCtls((PRGXFWIF_CLEANUP_CTL *) &s3DKCCBCmd.uCmdData.sCmdKickData.apsCleanupCtl,
											&s3DKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl,
											RGXFWIF_DM_3D,
											bKick3D,
											psRTDataCleanup,
											psZBuffer,
											psSBuffer,
											psMSAAScratchBuffer);
			*pbCommittedRefCounts3D = IMG_TRUE;
		}
		else
		{
			s3DKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;
		}


		HTBLOGK(HTB_SF_MAIN_KICK_3D,
				s3DKCCBCmd.uCmdData.sCmdKickData.psContext,
				ui323DCmdOffset);

#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
		RGX_HWPERF_HOST_ENQ(psRenderContext,
		                    OSGetCurrentClientProcessIDKM(),
		                    ui32FWCtx,
		                    ui32ExtJobRef,
		                    ui32IntJobRef,
		                    RGX_HWPERF_KICK_TYPE_3D,
		                    uiCheck3DFenceUID,
		                    uiUpdate3DFenceUID,
		                    ui64DeadlineInus,
		                    WORKEST_CYCLES_PREDICTION_GET(sWorkloadKickData3D));
#else
		RGX_HWPERF_HOST_ENQ(psRenderContext,
		                    OSGetCurrentClientProcessIDKM(),
		                    ui32FWCtx,
		                    ui32ExtJobRef,
		                    ui32IntJobRef,
		                    RGX_HWPERF_KICK_TYPE_3D,
		                    uiCheckFenceUID,
		                    uiUpdateFenceUID,
		                    ui64DeadlineInus,
		                    WORKEST_CYCLES_PREDICTION_GET(sWorkloadKickData3D));
#endif

		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError2 = RGXScheduleCommand(psRenderContext->psDeviceNode->pvDevice,
										RGXFWIF_DM_3D,
										&s3DKCCBCmd,
										sizeof(s3DKCCBCmd),
										ui32ClientCacheOpSeqNum,
										ui32PDumpFlags);
			if (eError2 != PVRSRV_ERROR_RETRY)
			{
				break;
			}
			OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();
	}

	/*
	 * Now check eError (which may have returned an error from our earlier calls
	 * to RGXCmdHelperAcquireCmdCCB) - we needed to process any flush command first
	 * so we check it now...
	 */
	if (eError != PVRSRV_OK )
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s: Failed, eError=%d, Line", __FUNCTION__, eError));
		goto fail_3dacquirecmd;
	}

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined (SUPPORT_FALLBACK_FENCE_SYNC)
#if defined(NO_HARDWARE)
	/* If NO_HARDWARE, signal the output fence's sync checkpoint and sync prim */
#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	if (psUpdateTASyncCheckpoint)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Signalling NOHW sync checkpoint [TA] <%p>, ID:%d, FwAddr=0x%x", __FUNCTION__, \
			(void*)psUpdateTASyncCheckpoint, SyncCheckpointGetId(psUpdateTASyncCheckpoint), SyncCheckpointGetFirmwareAddr(psUpdateTASyncCheckpoint)));
		SyncCheckpointSignalNoHW(psUpdateTASyncCheckpoint);
	}
	if (psTAFenceTimelineUpdateSync)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Updating NOHW sync prim [TA] <%p> to %d", __FUNCTION__, (void*)psTAFenceTimelineUpdateSync, ui32TAFenceTimelineUpdateValue));
		SyncPrimNoHwUpdate(psTAFenceTimelineUpdateSync, ui32TAFenceTimelineUpdateValue);
	}

	if (psUpdate3DSyncCheckpoint)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Signalling NOHW sync checkpoint [3D] <%p>, ID:%d, FwAddr=0x%x", __FUNCTION__, \
			(void*)psUpdate3DSyncCheckpoint, SyncCheckpointGetId(psUpdate3DSyncCheckpoint), SyncCheckpointGetFirmwareAddr(psUpdate3DSyncCheckpoint)));
		SyncCheckpointSignalNoHW(psUpdate3DSyncCheckpoint);
	}
	if (ps3DFenceTimelineUpdateSync)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Updating NOHW sync prim [3D] <%p> to %d", __FUNCTION__, (void*)ps3DFenceTimelineUpdateSync, ui323DFenceTimelineUpdateValue));
		SyncPrimNoHwUpdate(ps3DFenceTimelineUpdateSync, ui323DFenceTimelineUpdateValue);
	}
	SyncCheckpointNoHWUpdateTimelines(NULL);

#else
	if (psUpdateSyncCheckpoint)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Signalling NOHW sync checkpoint<%p>, ID:%d, FwAddr=0x%x", __FUNCTION__, (void*)psUpdateSyncCheckpoint, SyncCheckpointGetId(psUpdateSyncCheckpoint), SyncCheckpointGetFirmwareAddr(psUpdateSyncCheckpoint)));
		SyncCheckpointSignalNoHW(psUpdateSyncCheckpoint);
	}
	if (psFenceTimelineUpdateSync)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   Updating NOHW sync prim<%p> to %d", __FUNCTION__, (void*)psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue));
		SyncPrimNoHwUpdate(psFenceTimelineUpdateSync, ui32FenceTimelineUpdateValue);
	}
	SyncCheckpointNoHWUpdateTimelines(NULL);

#endif /* defined(PVRSRV_SYNC_SEPARATE_TIMELINES) */
#endif /* defined(NO_HARDWARE) */
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined (SUPPORT_FALLBACK_FENCE_SYNC) */

#if defined(SUPPORT_BUFFER_SYNC)
	if (psBufferSyncData)
	{
		CHKPT_DBG((PVR_DBG_ERROR, "%s:   calling pvr_buffer_sync_kick_succeeded(psBufferSyncData=<%p>)...", __FUNCTION__, (void*)psBufferSyncData));
		pvr_buffer_sync_kick_succeeded(psBufferSyncData);
	}
	if (apsBufferFenceSyncCheckpoints)
	{
		kfree(apsBufferFenceSyncCheckpoints);
	}
#endif /* defined(SUPPORT_BUFFER_SYNC) */

#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	*piUpdateTAFence = iUpdateTAFence;
	*piUpdate3DFence = iUpdate3DFence;
#else
	*piUpdateFence = iUpdateFence;
#endif

#if defined(SUPPORT_FALLBACK_FENCE_SYNC) || defined(SUPPORT_NATIVE_FENCE_SYNC)
	/* Drop the references taken on the sync checkpoints in the
	 * resolved input fence.
 	 * NOTE: 3D fence is always submitted, either via 3D or TA(PR).
 	 */
	#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	if (bKickTA)
	{
		SyncAddrListDeRefCheckpoints(ui32FenceTASyncCheckpointCount, apsFenceTASyncCheckpoints);
	}
	SyncAddrListDeRefCheckpoints(ui32Fence3DSyncCheckpointCount, apsFence3DSyncCheckpoints);

	if (pvTAUpdateFenceFinaliseData && (iUpdateTAFence != PVRSRV_NO_FENCE))
	{
		SyncCheckpointFinaliseFence(iUpdateTAFence, pvTAUpdateFenceFinaliseData);
	}
	if (pv3DUpdateFenceFinaliseData && (iUpdate3DFence != PVRSRV_NO_FENCE))
	{
		SyncCheckpointFinaliseFence(iUpdate3DFence, pv3DUpdateFenceFinaliseData);
	}
	#else
	SyncAddrListDeRefCheckpoints(ui32FenceSyncCheckpointCount, apsFenceSyncCheckpoints);
	if (pvUpdateFenceFinaliseData && (iUpdateFence != PVRSRV_NO_FENCE))
	{
		SyncCheckpointFinaliseFence(iUpdateFence, pvUpdateFenceFinaliseData);
	}
	#endif

	/* Free the memory that was allocated for the sync checkpoint list returned by ResolveFence() */
	#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	if (apsFenceTASyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFenceTASyncCheckpoints);
	}
	if (apsFence3DSyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFence3DSyncCheckpoints);
	}

	if (sTASyncData.paui32ClientUpdateValue)
	{
		OSFreeMem(sTASyncData.paui32ClientUpdateValue);
	}
	if (s3DSyncData.paui32ClientUpdateValue)
	{
		OSFreeMem(s3DSyncData.paui32ClientUpdateValue);
	}

	#else
	if (apsFenceSyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFenceSyncCheckpoints);
	}
	/* Free memory allocated to hold the internal list of update values */
	if (pui32IntAllocatedUpdateValues)
	{
		OSFreeMem(pui32IntAllocatedUpdateValues);
	}
	#endif
#endif
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psRenderContext->hLock);
#endif
	return PVRSRV_OK;

fail_3dacquirecmd:
fail_3dcmdinit:
fail_prcmdinit:
fail_prfencecmdinit:
fail_taacquirecmd:
fail_tacmdinit:
	SyncAddrListRollbackCheckpoints(psRenderContext->psDeviceNode, &psRenderContext->sSyncAddrListTAFence);
	SyncAddrListRollbackCheckpoints(psRenderContext->psDeviceNode, &psRenderContext->sSyncAddrListTAUpdate);
	SyncAddrListRollbackCheckpoints(psRenderContext->psDeviceNode, &psRenderContext->sSyncAddrList3DFence);
	SyncAddrListRollbackCheckpoints(psRenderContext->psDeviceNode, &psRenderContext->sSyncAddrList3DUpdate);
	/* Where a TA-only kick (ie no 3D) is submitted, the PR update will make use of the unused 3DUpdate list.
	 * If this has happened, performing a rollback on pauiClientPRUpdateUFOAddress will simply repeat what
	 * has already been done for the sSyncAddrList3DUpdate above and result in a double decrement of the
	 * sync checkpoint's hEnqueuedCCBCount, so we need to check before rolling back the PRUpdate.
	 */
	if (pauiClientPRUpdateUFOAddress && (pauiClientPRUpdateUFOAddress != psRenderContext->sSyncAddrList3DUpdate.pasFWAddrs))
	{
		SyncCheckpointRollbackFromUFO(psRenderContext->psDeviceNode, pauiClientPRUpdateUFOAddress->ui32Addr);
	}

#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined (SUPPORT_FALLBACK_FENCE_SYNC)
	#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
fail_alloc_update_values_mem_3D:
	if (sTASyncData.paui32ClientUpdateValue)
	{
		OSFreeMem(sTASyncData.paui32ClientUpdateValue);
	}
	/* FIXME: sTASyncData.paui32ClientPRUpdateValue points to the same buffer, needs a review */
fail_alloc_update_values_mem_TA:
	if (iUpdateTAFence != PVRSRV_NO_FENCE)
	{
		SyncCheckpointRollbackFenceData(iUpdateTAFence, pvTAUpdateFenceFinaliseData);
	}
	if (iUpdate3DFence != PVRSRV_NO_FENCE)
	{
		SyncCheckpointRollbackFenceData(iUpdate3DFence, pv3DUpdateFenceFinaliseData);
	}
	#else
fail_alloc_update_values_mem:
	if (iUpdateFence != PVRSRV_NO_FENCE)
	{
		SyncCheckpointRollbackFenceData(iUpdateFence, pvUpdateFenceFinaliseData);
	}
	#endif
fail_create_output_fence:
	/* Drop the references taken on the sync checkpoints in the
	 * resolved input fence.
	 * NOTE: 3D fence is always submitted, either via 3D or TA(PR).
	 */
	#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	if (bKickTA)
	{
		SyncAddrListDeRefCheckpoints(ui32FenceTASyncCheckpointCount, apsFenceTASyncCheckpoints);
	}
	SyncAddrListDeRefCheckpoints(ui32Fence3DSyncCheckpointCount, apsFence3DSyncCheckpoints);
	#else
	SyncAddrListDeRefCheckpoints(ui32FenceSyncCheckpointCount, apsFenceSyncCheckpoints);
	#endif
fail_resolve_input_fence:
fail_fdsync:
#endif /* defined(SUPPORT_NATIVE_FENCE_SYNC) || defined (SUPPORT_FALLBACK_FENCE_SYNC) */

#if defined(SUPPORT_BUFFER_SYNC)
	if (psBufferSyncData)
	{
		pvr_buffer_sync_kick_failed(psBufferSyncData);
	}
	if (apsBufferFenceSyncCheckpoints)
	{
		kfree(apsBufferFenceSyncCheckpoints);
	}
#endif /* defined(SUPPORT_BUFFER_SYNC) */

err_pr_fence_address:
err_populate_sync_addr_list_3d_update:
err_populate_sync_addr_list_3d_fence:
err_populate_sync_addr_list_ta_update:
err_populate_sync_addr_list_ta_fence:
#if defined(SUPPORT_NATIVE_FENCE_SYNC)
	/* Free the memory that was allocated for the sync checkpoint list returned by ResolveFence() */
	#if defined(PVRSRV_SYNC_SEPARATE_TIMELINES)
	if (apsFenceTASyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFenceTASyncCheckpoints);
	}
	if (apsFence3DSyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFence3DSyncCheckpoints);
	}
	if (sTASyncData.paui32ClientUpdateValue)
	{
		OSFreeMem(sTASyncData.paui32ClientUpdateValue);
	}
	if (s3DSyncData.paui32ClientUpdateValue)
	{
		OSFreeMem(s3DSyncData.paui32ClientUpdateValue);
	}
	#else
	if (apsFenceSyncCheckpoints)
	{
		SyncCheckpointFreeCheckpointListMem(apsFenceSyncCheckpoints);
	}
	/* Free memory allocated to hold the internal list of update values */
	if (pui32IntAllocatedUpdateValues)
	{
		OSFreeMem(pui32IntAllocatedUpdateValues);
	}
	#endif
#endif
	PVR_ASSERT(eError != PVRSRV_OK);
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psRenderContext->hLock);
#endif
	return eError;
}

PVRSRV_ERROR PVRSRVRGXSetRenderContextPriorityKM(CONNECTION_DATA *psConnection,
                                                 PVRSRV_DEVICE_NODE * psDeviceNode,
												 RGX_SERVER_RENDER_CONTEXT *psRenderContext,
												 IMG_UINT32 ui32Priority)
{
	PVRSRV_ERROR eError;

	PVR_UNREFERENCED_PARAMETER(psDeviceNode);

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockAcquire(psRenderContext->hLock);
#endif

	if (psRenderContext->sTAData.ui32Priority != ui32Priority)
	{
		eError = ContextSetPriority(psRenderContext->sTAData.psServerCommonContext,
									psConnection,
									psRenderContext->psDeviceNode->pvDevice,
									ui32Priority,
									RGXFWIF_DM_TA);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set the priority of the TA part of the rendercontext (%s)", __FUNCTION__, PVRSRVGetErrorStringKM(eError)));
			goto fail_tacontext;
		}
		psRenderContext->sTAData.ui32Priority = ui32Priority;
	}

	if (psRenderContext->s3DData.ui32Priority != ui32Priority)
	{
		eError = ContextSetPriority(psRenderContext->s3DData.psServerCommonContext,
									psConnection,
									psRenderContext->psDeviceNode->pvDevice,
									ui32Priority,
									RGXFWIF_DM_3D);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to set the priority of the 3D part of the rendercontext (%s)", __FUNCTION__, PVRSRVGetErrorStringKM(eError)));
			goto fail_3dcontext;
		}
		psRenderContext->s3DData.ui32Priority = ui32Priority;
	}

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psRenderContext->hLock);
#endif
	return PVRSRV_OK;

fail_3dcontext:
fail_tacontext:
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockRelease(psRenderContext->hLock);
#endif
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}


/*
 * PVRSRVRGXGetLastRenderContextResetReasonKM
 */
PVRSRV_ERROR PVRSRVRGXGetLastRenderContextResetReasonKM(RGX_SERVER_RENDER_CONTEXT *psRenderContext,
                                                        IMG_UINT32 *peLastResetReason,
                                                        IMG_UINT32 *pui32LastResetJobRef)
{
	RGX_SERVER_RC_TA_DATA         *psRenderCtxTAData;
	RGX_SERVER_RC_3D_DATA         *psRenderCtx3DData;
	RGX_SERVER_COMMON_CONTEXT     *psCurrentServerTACommonCtx, *psCurrentServer3DCommonCtx;
	RGXFWIF_CONTEXT_RESET_REASON  eLastTAResetReason, eLast3DResetReason;
	IMG_UINT32                    ui32LastTAResetJobRef, ui32Last3DResetJobRef;

	PVR_ASSERT(psRenderContext != NULL);
	PVR_ASSERT(peLastResetReason != NULL);
	PVR_ASSERT(pui32LastResetJobRef != NULL);

	psRenderCtxTAData          = &(psRenderContext->sTAData);
	psCurrentServerTACommonCtx = psRenderCtxTAData->psServerCommonContext;
	psRenderCtx3DData          = &(psRenderContext->s3DData);
	psCurrentServer3DCommonCtx = psRenderCtx3DData->psServerCommonContext;

	/* Get the last reset reasons from both the TA and 3D so they are reset... */
	eLastTAResetReason = FWCommonContextGetLastResetReason(psCurrentServerTACommonCtx, &ui32LastTAResetJobRef);
	eLast3DResetReason = FWCommonContextGetLastResetReason(psCurrentServer3DCommonCtx, &ui32Last3DResetJobRef);

	/* Combine the reset reason from TA and 3D into one... */
	*peLastResetReason    = (IMG_UINT32) eLast3DResetReason;
	*pui32LastResetJobRef = ui32Last3DResetJobRef;
	if (eLast3DResetReason == RGXFWIF_CONTEXT_RESET_REASON_NONE  ||
	    ((eLast3DResetReason == RGXFWIF_CONTEXT_RESET_REASON_INNOCENT_LOCKUP  ||
	      eLast3DResetReason == RGXFWIF_CONTEXT_RESET_REASON_INNOCENT_OVERRUNING)  &&
	     (eLastTAResetReason == RGXFWIF_CONTEXT_RESET_REASON_GUILTY_LOCKUP  ||
	      eLastTAResetReason == RGXFWIF_CONTEXT_RESET_REASON_GUILTY_OVERRUNING)))
	{
		*peLastResetReason    = eLastTAResetReason;
		*pui32LastResetJobRef = ui32LastTAResetJobRef;
	}

	return PVRSRV_OK;
}


/*
 * PVRSRVRGXGetPartialRenderCountKM
 */
PVRSRV_ERROR PVRSRVRGXGetPartialRenderCountKM(DEVMEM_MEMDESC *psHWRTDataMemDesc,
											  IMG_UINT32 *pui32NumPartialRenders)
{
	RGXFWIF_HWRTDATA *psHWRTData;
	PVRSRV_ERROR eError;

	eError = DevmemAcquireCpuVirtAddr(psHWRTDataMemDesc, (void **)&psHWRTData);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRGXGetPartialRenderCountKM: Failed to map Firmware Render Target Data (%u)", eError));
		return eError;
	}

	*pui32NumPartialRenders = psHWRTData->ui32NumPartialRenders;

	DevmemReleaseCpuVirtAddr(psHWRTDataMemDesc);

	return PVRSRV_OK;
}

void CheckForStalledRenderCtxt(PVRSRV_RGXDEV_INFO *psDevInfo,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	DLLIST_NODE *psNode, *psNext;
	OSWRLockAcquireRead(psDevInfo->hRenderCtxListLock);
	dllist_foreach_node(&psDevInfo->sRenderCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_RENDER_CONTEXT *psCurrentServerRenderCtx =
			IMG_CONTAINER_OF(psNode, RGX_SERVER_RENDER_CONTEXT, sListNode);

		DumpStalledFWCommonContext(psCurrentServerRenderCtx->sTAData.psServerCommonContext,
								   pfnDumpDebugPrintf, pvDumpDebugFile);
		DumpStalledFWCommonContext(psCurrentServerRenderCtx->s3DData.psServerCommonContext,
								   pfnDumpDebugPrintf, pvDumpDebugFile);
	}
	OSWRLockReleaseRead(psDevInfo->hRenderCtxListLock);
}

IMG_UINT32 CheckForStalledClientRenderCtxt(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	DLLIST_NODE *psNode, *psNext;
	IMG_UINT32 ui32ContextBitMask = 0;

	OSWRLockAcquireRead(psDevInfo->hRenderCtxListLock);

	dllist_foreach_node(&psDevInfo->sRenderCtxtListHead, psNode, psNext)
	{
		RGX_SERVER_RENDER_CONTEXT *psCurrentServerRenderCtx =
			IMG_CONTAINER_OF(psNode, RGX_SERVER_RENDER_CONTEXT, sListNode);
		if (NULL != psCurrentServerRenderCtx->sTAData.psServerCommonContext)
		{
			if (CheckStalledClientCommonContext(psCurrentServerRenderCtx->sTAData.psServerCommonContext, RGX_KICK_TYPE_DM_TA) == PVRSRV_ERROR_CCCB_STALLED)
			{
				ui32ContextBitMask |= RGX_KICK_TYPE_DM_TA;
			}
		}

		if (NULL != psCurrentServerRenderCtx->s3DData.psServerCommonContext)
		{
			if (CheckStalledClientCommonContext(psCurrentServerRenderCtx->s3DData.psServerCommonContext, RGX_KICK_TYPE_DM_3D) == PVRSRV_ERROR_CCCB_STALLED)
			{
				ui32ContextBitMask |= RGX_KICK_TYPE_DM_3D;
			}
		}
	}

	OSWRLockReleaseRead(psDevInfo->hRenderCtxListLock);
	return ui32ContextBitMask;
}

/******************************************************************************
 End of file (rgxta3d.c)
******************************************************************************/
