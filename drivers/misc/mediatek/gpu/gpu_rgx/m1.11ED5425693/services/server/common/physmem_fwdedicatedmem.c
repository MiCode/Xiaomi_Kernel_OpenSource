/*************************************************************************/ /*!
@File
@Title          PMR functions for Trusted Device firmware code memory
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Part of the memory management. This module is responsible for
                implementing the function callbacks for physical memory
                imported from a trusted environment. The driver cannot acquire
                CPU mappings for this secure memory.
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

#include "pvr_debug.h"
#include "pvrsrv.h"
#include "physmem_fwdedicatedmem.h"
#include "physheap.h"
#include "rgxdevice.h"
#include "rgx_bvnc_defs_km.h"
#include "devicemem_server_utils.h"

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
#include "ri_server.h"
#endif

#if !defined(NO_HARDWARE)

typedef struct _PMR_FWMEM_DATA_ {
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PHYS_HEAP          *psFWMemPhysHeap;
	IMG_CPU_PHYADDR    sCpuPAddr;
	IMG_DEV_PHYADDR    sDevPAddr;
	PMR_LOG2ALIGN_T    uiLog2Align;
	IMG_UINT64         ui64Size;
} PMR_FWMEM_DATA;


/*
 * Implementation of callback functions
 */

static PVRSRV_ERROR
PMRLockPhysAddressesFWMem(PMR_IMPL_PRIVDATA pvPriv)
{
	/* There is nothing to do as we control LMA physical memory */
	PVR_UNREFERENCED_PARAMETER(pvPriv);

	return PVRSRV_OK;
}

static PVRSRV_ERROR
PMRUnlockPhysAddressesFWMem(PMR_IMPL_PRIVDATA pvPriv)
{
	/* There is nothing to do as we control LMA physical memory */
	PVR_UNREFERENCED_PARAMETER(pvPriv);

	return PVRSRV_OK;
}

static PVRSRV_ERROR PMRSysPhysAddrFWMem(PMR_IMPL_PRIVDATA pvPriv,
                                        IMG_UINT32 ui32Log2PageSize,
                                        IMG_UINT32 ui32NumOfPages,
                                        IMG_DEVMEM_OFFSET_T *puiOffset,
                                        IMG_BOOL *pbValid,
                                        IMG_DEV_PHYADDR *psDevPAddr)
{
	PMR_FWMEM_DATA *psPrivData = pvPriv;
	IMG_UINT32 i;

	PVR_UNREFERENCED_PARAMETER(ui32Log2PageSize);

	for (i = 0; i < ui32NumOfPages; i++)
	{
		if (pbValid[i])
		{
			psDevPAddr[i].uiAddr = psPrivData->sDevPAddr.uiAddr + puiOffset[i];
		}
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR
PMRAcquireKernelMappingDataFWMem(PMR_IMPL_PRIVDATA pvPriv,
                                 size_t uiOffset,
                                 size_t uiSize,
                                 void **ppvKernelAddressOut,
                                 IMG_HANDLE *phHandleOut,
                                 PMR_FLAGS_T ulFlags)
{
	PMR_FWMEM_DATA *psPrivData = pvPriv;
	void *pvKernLinAddr = NULL;
	IMG_UINT32 ui32CPUCacheFlags;
	PVRSRV_ERROR eError;

	eError = DevmemCPUCacheMode(psPrivData->psDeviceNode, ulFlags, &ui32CPUCacheFlags);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	PVR_UNREFERENCED_PARAMETER(uiSize);

	pvKernLinAddr = OSMapPhysToLin(psPrivData->sCpuPAddr, psPrivData->ui64Size, ui32CPUCacheFlags);

	if (pvKernLinAddr == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	*ppvKernelAddressOut = ((IMG_CHAR *) pvKernLinAddr) + uiOffset;
	*phHandleOut = pvKernLinAddr;

	return PVRSRV_OK;
}

static void
PMRReleaseKernelMappingDataFWMem(PMR_IMPL_PRIVDATA pvPriv,
                                 IMG_HANDLE hHandle)
{
	PMR_FWMEM_DATA *psPrivData = pvPriv;
	void *pvKernLinAddr = hHandle;

	OSUnMapPhysToLin(pvKernLinAddr, (size_t)psPrivData->ui64Size, 0);
}

static PVRSRV_ERROR PMRFinalizeFWMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_FWMEM_DATA *psPrivData = NULL;

	psPrivData = pvPriv;
	RA_Free(psPrivData->psDeviceNode->psDedicatedFWMemArena,
	        psPrivData->sDevPAddr.uiAddr);
	OSFreeMem(psPrivData);

	return PVRSRV_OK;
}

static PMR_IMPL_FUNCTAB _sPMRFWMemFuncTab = {
	.pfnLockPhysAddresses        = &PMRLockPhysAddressesFWMem,
	.pfnUnlockPhysAddresses      = &PMRUnlockPhysAddressesFWMem,
	.pfnDevPhysAddr              = &PMRSysPhysAddrFWMem,
	.pfnAcquireKernelMappingData = &PMRAcquireKernelMappingDataFWMem,
	.pfnReleaseKernelMappingData = &PMRReleaseKernelMappingDataFWMem,
	.pfnFinalize                 = &PMRFinalizeFWMem,
};


/*
 * Public functions
 */
PVRSRV_ERROR PhysmemNewFWDedicatedMemPMR(PVRSRV_DEVICE_NODE *psDevNode,
                                         IMG_DEVMEM_SIZE_T uiSize,
                                         PMR_LOG2ALIGN_T uiLog2Align,
                                         PVRSRV_MEMALLOCFLAGS_T uiFlags,
                                         PMR **ppsPMRPtr)
{
	PMR_FWMEM_DATA *psPrivData = NULL;
	PMR *psPMR = NULL;
	RA_BASE_T uiCardAddr = 0;
	RA_LENGTH_T uiActualSize = 0;
	IMG_UINT32 uiMappingTable = 0;
	PMR_FLAGS_T uiPMRFlags;
	PVRSRV_ERROR eError;

	/* In this instance, we simply pass flags straight through. Generally,
	 * uiFlags can include things that control the PMR factory, but we
	 * don't need any such thing (at the time of writing!), and our caller
	 * specifies all PMR flags so we don't need to adjust what was given
	 * to us.
	 */
	uiPMRFlags = (PMR_FLAGS_T)(uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK);

	/* Check no significant bits were lost in cast due to different bit
	 * widths for flags.
	 */
	PVR_ASSERT(uiPMRFlags == (uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK));

	if (psDevNode->psDedicatedFWMemHeap == NULL || psDevNode->psDedicatedFWMemArena == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Invalid params (phys heap %p, arena %p)",
				 __func__,
				 psDevNode->psDedicatedFWMemHeap,
				 psDevNode->psDedicatedFWMemArena));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	eError = RA_Alloc(psDevNode->psDedicatedFWMemArena,
	                  uiSize,
	                  RA_NO_IMPORT_MULTIPLIER,
	                  0,                       /* No flags */
	                  1ULL << uiLog2Align,
	                  "FW_mem_alloc",
	                  &uiCardAddr,
	                  &uiActualSize,
	                  NULL);                   /* No private handle */

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Memory allocation failed (%u)",
				 __func__, eError));
		return eError;
	}

	psPrivData = OSAllocZMem(sizeof(PMR_FWMEM_DATA));

	if (psPrivData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto errorOnAllocData;
	}

	/*
	 * uiLog2Align is only used to get memory with the correct alignment.
	 * The page size is still determined by the OS.
	 */
	psPrivData->uiLog2Align = OSGetPageShift();
	psPrivData->psFWMemPhysHeap = psDevNode->psDedicatedFWMemHeap;
	psPrivData->ui64Size = uiSize;
	psPrivData->psDeviceNode = psDevNode;
	psPrivData->sDevPAddr.uiAddr = uiCardAddr;

	PhysHeapDevPAddrToCpuPAddr(psPrivData->psFWMemPhysHeap,
	                           1,
	                           &psPrivData->sCpuPAddr,
	                           &psPrivData->sDevPAddr);

	eError = PMRCreatePMR(psDevNode,
	                      psPrivData->psFWMemPhysHeap,
	                      psPrivData->ui64Size,
	                      psPrivData->ui64Size,
	                      1,                 /* ui32NumPhysChunks */
	                      1,                 /* ui32NumVirtChunks */
	                      &uiMappingTable,   /* pui32MappingTable (not used) */
	                      uiLog2Align,       /* uiLog2ContiguityGuarantee */
	                      uiPMRFlags,
	                      "FWMEM_PMR",
	                      &_sPMRFWMemFuncTab,
	                      psPrivData,
	                      PMR_TYPE_LMA,
	                      &psPMR,
	                      PDUMP_NONE);
	if (eError != PVRSRV_OK)
	{
		goto errorOnCreatePMR;
	}

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
	eError = RIWritePMREntryKM(psPMR);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING,
		         "%s: Failed to write PMR entry (%s)",
		         __func__, PVRSRVGetErrorString(eError)));
	}
#endif

	*ppsPMRPtr = psPMR;
	return PVRSRV_OK;

errorOnCreatePMR:
	OSFreeMem(psPrivData);
errorOnAllocData:
	RA_Free(psDevNode->psDedicatedFWMemArena,
	        uiCardAddr);
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

PVRSRV_ERROR PhysmemInitFWDedicatedMem(PVRSRV_DEVICE_NODE *psDeviceNode,
                                       PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_UINT64 ui64Size;
	PVRSRV_ERROR eError;
	RGX_DATA *psRGXData = (RGX_DATA *)psDevConfig->hDevData;

	if (!psRGXData->bHasFWMemPhysHeap)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Dedicated FW memory not available", __func__));
		return PVRSRV_ERROR_NOT_IMPLEMENTED;
	}

	eError = PhysHeapAcquire(psRGXData->uiFWMemPhysHeapID,
	                         &psDeviceNode->psDedicatedFWMemHeap);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to acquire dedicated FW memory physical heap",
				 __func__));
		goto errorOnPhysHeapAcquire;
	}

	if (PhysHeapGetType(psDeviceNode->psDedicatedFWMemHeap) != PHYS_HEAP_TYPE_LMA ||
	    PhysHeapNumberOfRegions(psDeviceNode->psDedicatedFWMemHeap) != 1)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Wrong heap details: type %u, number of regions %u",
				 __func__,
				 PhysHeapGetType(psDeviceNode->psDedicatedFWMemHeap),
				 PhysHeapNumberOfRegions(psDeviceNode->psDedicatedFWMemHeap)));
		eError = PVRSRV_ERROR_INVALID_HEAP;
		goto errorOnValidatePhysHeap;
	}

	PhysHeapRegionGetSize(psDeviceNode->psDedicatedFWMemHeap, 0, &ui64Size);
	PhysHeapRegionGetDevPAddr(psDeviceNode->psDedicatedFWMemHeap, 0, &sDevPAddr);

	psDeviceNode->psDedicatedFWMemArena =
		RA_Create("Dedicated_FW_mem",
				  OSGetPageShift(),
				  RA_LOCKCLASS_0,
				  NULL, NULL, NULL,
				  IMG_FALSE);

	if (psDeviceNode->psDedicatedFWMemArena == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create dedicated FW memory arena", __func__));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto errorOnRACreate;
	}

	if (!RA_Add(psDeviceNode->psDedicatedFWMemArena,
				(RA_BASE_T)sDevPAddr.uiAddr,
				(RA_LENGTH_T)ui64Size,
				0, NULL))
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to add memory to dedicated FW memory arena",
				 __func__));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto errorOnRAAdd;
	}

	return PVRSRV_OK;

errorOnRAAdd:
	RA_Delete(psDeviceNode->psDedicatedFWMemArena);
errorOnRACreate:
errorOnValidatePhysHeap:
	PhysHeapRelease(psDeviceNode->psDedicatedFWMemHeap);
errorOnPhysHeapAcquire:
	return eError;
}

void PhysmemDeinitFWDedicatedMem(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	RA_Delete(psDeviceNode->psDedicatedFWMemArena);
	PhysHeapRelease(psDeviceNode->psDedicatedFWMemHeap);
}

#else /* !defined(NO_HARDWARE) */

#include "physmem_osmem.h"

typedef struct _PMR_FWDEDICATEDMEM_DATA_ {
	PHYS_HEAP  *psFWMemPhysHeap;
	PMR        *psOSMemPMR;
	IMG_UINT32 ui32Log2PageSize;
} PMR_FWDEDICATEDMEM_DATA;


/*
 * Implementation of callback functions
 */

static PVRSRV_ERROR
PMRLockPhysAddressesFWMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_FWDEDICATEDMEM_DATA *psPrivData = pvPriv;

	return PMRLockSysPhysAddresses(psPrivData->psOSMemPMR);
}

static PVRSRV_ERROR
PMRUnlockPhysAddressesFWMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_FWDEDICATEDMEM_DATA *psPrivData = pvPriv;

	return PMRUnlockSysPhysAddresses(psPrivData->psOSMemPMR);
}

static PVRSRV_ERROR
PMRSysPhysAddrFWMem(PMR_IMPL_PRIVDATA pvPriv,
                    IMG_UINT32 ui32Log2PageSize,
                    IMG_UINT32 ui32NumOfPages,
                    IMG_DEVMEM_OFFSET_T *puiOffset,
                    IMG_BOOL *pbValid,
                    IMG_DEV_PHYADDR *psDevPAddr)
{
	PMR_FWDEDICATEDMEM_DATA *psPrivData = pvPriv;

	if (psPrivData->ui32Log2PageSize != ui32Log2PageSize)
	{
		return PVRSRV_ERROR_PMR_INCOMPATIBLE_CONTIGUITY;
	}

	/* On the assumption that this PMR was created with
	 * NumPhysChunks == NumVirtChunks then
	 * puiOffset[0] == uiLogicalOffset
	 */
	return PMR_DevPhysAddr(psPrivData->psOSMemPMR,
	                       ui32Log2PageSize,
	                       ui32NumOfPages,
	                       puiOffset[0],
	                       psDevPAddr,
	                       pbValid);
}

static PVRSRV_ERROR
PMRAcquireKernelMappingDataFWMem(PMR_IMPL_PRIVDATA pvPriv,
                                 size_t uiOffset,
                                 size_t uiSize,
                                 void **ppvKernelAddressOut,
                                 IMG_HANDLE *phHandleOut,
                                 PMR_FLAGS_T ulFlags)
{
	PMR_FWDEDICATEDMEM_DATA *psPrivData = pvPriv;
	size_t uiLengthOut;

	PVR_UNREFERENCED_PARAMETER(ulFlags);

	return PMRAcquireKernelMappingData(psPrivData->psOSMemPMR,
	                                   uiOffset,
	                                   uiSize,
	                                   ppvKernelAddressOut,
	                                   &uiLengthOut,
	                                   phHandleOut);
}

static void
PMRReleaseKernelMappingDataFWMem(PMR_IMPL_PRIVDATA pvPriv,
                                 IMG_HANDLE hHandle)
{
	PMR_FWDEDICATEDMEM_DATA *psPrivData = pvPriv;

	PMRReleaseKernelMappingData(psPrivData->psOSMemPMR, hHandle);
}

static PVRSRV_ERROR PMRFinalizeFWMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_FWDEDICATEDMEM_DATA *psPrivData = pvPriv;

	PMRUnrefPMR(psPrivData->psOSMemPMR);
	PhysHeapRelease(psPrivData->psFWMemPhysHeap);
	OSFreeMem(psPrivData);

	return PVRSRV_OK;
}

static PMR_IMPL_FUNCTAB _sPMRFWFuncTab = {
	.pfnLockPhysAddresses        = &PMRLockPhysAddressesFWMem,
	.pfnUnlockPhysAddresses      = &PMRUnlockPhysAddressesFWMem,
	.pfnDevPhysAddr              = &PMRSysPhysAddrFWMem,
	.pfnAcquireKernelMappingData = &PMRAcquireKernelMappingDataFWMem,
	.pfnReleaseKernelMappingData = &PMRReleaseKernelMappingDataFWMem,
	.pfnFinalize                 = &PMRFinalizeFWMem,
};


/*
 * Public functions
 */
PVRSRV_ERROR PhysmemNewFWDedicatedMemPMR(PVRSRV_DEVICE_NODE *psDevNode,
                                         IMG_DEVMEM_SIZE_T uiSize,
                                         PMR_LOG2ALIGN_T uiLog2Align,
                                         PVRSRV_MEMALLOCFLAGS_T uiFlags,
                                         PMR **ppsPMRPtr)
{
	RGX_DATA *psRGXData = (RGX_DATA *)(psDevNode->psDevConfig->hDevData);
	PMR_FWDEDICATEDMEM_DATA *psPrivData = NULL;
	PMR *psPMR = NULL;
	PMR *psOSPMR = NULL;
	IMG_UINT32 uiMappingTable = 0;
	PMR_FLAGS_T uiPMRFlags;
	PVRSRV_ERROR eError;

	/* In this instance, we simply pass flags straight through. Generally,
	 * uiFlags can include things that control the PMR factory, but we
	 * don't need any such thing (at the time of writing!), and our caller
	 * specifies all PMR flags so we don't need to adjust what was given
	 * to us.
	 */
	uiPMRFlags = (PMR_FLAGS_T)(uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK);

	/* Check no significant bits were lost in cast due to different bit
	 * widths for flags.
	 */
	PVR_ASSERT(uiPMRFlags == (uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK));

	psPrivData = OSAllocZMem(sizeof(PMR_FWDEDICATEDMEM_DATA));
	if (psPrivData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto errorOnAllocData;
	}

	/* Get required info for the dedicated FW memory physical heap */
	if (!psRGXData->bHasFWMemPhysHeap)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Physical heap not available!", __func__));
		eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
		goto errorOnAcquireHeap;
	}
	eError = PhysHeapAcquire(psRGXData->uiFWMemPhysHeapID,
	                         &psPrivData->psFWMemPhysHeap);
	if(eError != PVRSRV_OK) goto errorOnAcquireHeap;

	/* The alignment requested by the caller is only used to generate the
	 * secure FW allocation pdump command with the correct alignment.
	 * Internally we use another PMR with OS page alignment.
	 */
	psPrivData->ui32Log2PageSize = OSGetPageShift();

	/* Note that this PMR is only used to copy the FW blob to memory and
	 * to dump this memory to pdump, it doesn't need to have the alignment
	 * requested by the caller.
	 */
	eError = PhysmemNewOSRamBackedPMR(psDevNode,
	                                  uiSize,
	                                  uiSize,
	                                  1,                 /* ui32NumPhysChunks */
	                                  1,                 /* ui32NumVirtChunks */
	                                  &uiMappingTable,
	                                  psPrivData->ui32Log2PageSize,
	                                  uiFlags,
	                                  "DEDICATEDFWMEM_OSMEM",
	                                  OSGetCurrentClientProcessIDKM(),
	                                  &psOSPMR);
	if (eError != PVRSRV_OK)
	{
		goto errorOnCreateOSPMR;
	}

	/* This is the primary PMR dumped with correct memspace and alignment */
	eError = PMRCreatePMR(psDevNode,
	                      psPrivData->psFWMemPhysHeap,
	                      uiSize,
	                      uiSize,
	                      1,                 /* ui32NumPhysChunks */
	                      1,                 /* ui32NumVirtChunks */
	                      &uiMappingTable,   /* pui32MappingTable (not used) */
	                      uiLog2Align,       /* uiLog2ContiguityGuarantee */
	                      uiPMRFlags,
	                      "DEDICATEDFWMEM_PMR",
	                      &_sPMRFWFuncTab,
	                      psPrivData,
	                      PMR_TYPE_OSMEM,
	                      &psPMR,
	                      PDUMP_NONE);
	if (eError != PVRSRV_OK)
	{
		goto errorOnCreatePMR;
	}

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
	eError = RIWritePMREntryKM(psPMR);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING,
		         "%s: Failed to write PMR entry (%s)",
		         __func__, PVRSRVGetErrorString(eError)));
	}
#endif

	psPrivData->psOSMemPMR = psOSPMR;
	*ppsPMRPtr = psPMR;

	return PVRSRV_OK;

errorOnCreatePMR:
	PMRUnrefPMR(psOSPMR);

errorOnCreateOSPMR:
	PhysHeapRelease(psPrivData->psFWMemPhysHeap);

errorOnAcquireHeap:
	OSFreeMem(psPrivData);

errorOnAllocData:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

#endif
