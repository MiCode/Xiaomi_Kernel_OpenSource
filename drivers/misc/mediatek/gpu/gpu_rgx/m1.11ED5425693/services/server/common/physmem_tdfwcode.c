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
#include "physmem_tdfwcode.h"
#include "physheap.h"
#include "rgxdevice.h"
#include "rgx_bvnc_defs_km.h"

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
#include "ri_server.h"
#endif


#if !defined(NO_HARDWARE)

typedef struct _PMR_TDFWCODE_DATA_ {
	PHYS_HEAP       *psTDFWCodePhysHeap;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	PMR_LOG2ALIGN_T uiLog2Align;
	IMG_UINT64      ui64Size;
} PMR_TDFWCODE_DATA;


/*
 * Implementation of callback functions
 */

static PVRSRV_ERROR PMRSysPhysAddrTDFWCodeMem(PMR_IMPL_PRIVDATA pvPriv,
                                              IMG_UINT32 ui32Log2PageSize,
                                              IMG_UINT32 ui32NumOfPages,
                                              IMG_DEVMEM_OFFSET_T *puiOffset,
                                              IMG_BOOL *pbValid,
                                              IMG_DEV_PHYADDR *psDevPAddr)
{
	PMR_TDFWCODE_DATA *psPrivData = pvPriv;
	IMG_UINT32 i;

	if (psPrivData->uiLog2Align != ui32Log2PageSize)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Incompatible contiguity (requested %u, got %u)",
				 __func__, ui32Log2PageSize, psPrivData->uiLog2Align));
		return PVRSRV_ERROR_PMR_INCOMPATIBLE_CONTIGUITY;
	}

	for (i = 0; i < ui32NumOfPages; i++)
	{
		if (pbValid[i])
		{
			psDevPAddr[i].uiAddr = psPrivData->sDevPAddr.uiAddr + puiOffset[i];
		}
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR PMRFinalizeTDFWCodeMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_TDFWCODE_DATA *psPrivData = NULL;

	psPrivData = pvPriv;
	PhysHeapRelease(psPrivData->psTDFWCodePhysHeap);
	OSFreeMem(psPrivData);

	return PVRSRV_OK;
}

static PMR_IMPL_FUNCTAB _sPMRTDFWCodeFuncTab = {
	.pfnDevPhysAddr = &PMRSysPhysAddrTDFWCodeMem,
	.pfnFinalize    = &PMRFinalizeTDFWCodeMem,
};


/*
 * Public functions
 */
PVRSRV_ERROR PhysmemNewTDFWCodePMR(PVRSRV_DEVICE_NODE *psDevNode,
                                   IMG_DEVMEM_SIZE_T uiSize,
                                   PMR_LOG2ALIGN_T uiLog2Align,
                                   PVRSRV_MEMALLOCFLAGS_T uiFlags,
                                   PVRSRV_TD_FW_MEM_REGION eRegion,
                                   PMR **ppsPMRPtr)
{
	PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;
	RGX_DATA *psRGXData = (RGX_DATA *)(psDevConfig->hDevData);
	PMR_TDFWCODE_DATA *psPrivData = NULL;
	PMR *psPMR = NULL;
	IMG_UINT32 uiMappingTable = 0;
	PMR_FLAGS_T uiPMRFlags;
	IMG_UINT32 ui32CacheLineSize = 0;
	IMG_UINT32 ui32FWCodeRegionId;
	PVRSRV_ERROR eError;

	ui32CacheLineSize = GET_ROGUE_CACHE_LINE_SIZE(PVRSRV_GET_DEVICE_FEATURE_VALUE(psDevNode, SLC_CACHE_LINE_SIZE_BITS));

	/* In this instance, we simply pass flags straight through.
	 * Generically, uiFlags can include things that control the PMR
	 * factory, but we don't need any such thing (at the time of
	 * writing!), and our caller specifies all PMR flags so we don't
	 * need to meddle with what was given to us.
	 */
	uiPMRFlags = (PMR_FLAGS_T)(uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK);

	/* Check no significant bits were lost in cast due to different bit widths for flags */
	PVR_ASSERT(uiPMRFlags == (uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK));

	/* Many flags can be dropped as the driver cannot access this memory
	 * and it is assumed that the trusted zone is physically contiguous
	 */
	uiPMRFlags &= ~(PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE |
	                PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
	                PVRSRV_MEMALLOCFLAG_POISON_ON_ALLOC |
	                PVRSRV_MEMALLOCFLAG_POISON_ON_FREE |
	                PVRSRV_MEMALLOCFLAGS_CPU_MMUFLAGSMASK |
	                PVRSRV_MEMALLOCFLAG_SPARSE_NO_DUMMY_BACKING);

	psPrivData = OSAllocZMem(sizeof(PMR_TDFWCODE_DATA));
	if (psPrivData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto errorOnAllocData;
	}

	/* Get required info for the TD Meta Code physical heap */
	if (!psRGXData->bHasTDFWCodePhysHeap)
	{
		PVR_DPF((PVR_DBG_ERROR, "Trusted Device physical heap not available!"));
		eError = PVRSRV_ERROR_REQUEST_TDFWCODE_PAGES_FAIL;
		goto errorOnAcquireHeap;
	}

	ui32FWCodeRegionId = eRegion;

	eError = PhysHeapAcquire(psRGXData->uiTDFWCodePhysHeapID,
	                         &psPrivData->psTDFWCodePhysHeap);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Could not acquire secure physical heap %u",
				 __func__, psRGXData->uiTDFWCodePhysHeapID));
		goto errorOnAcquireHeap;
	}

	eError = PhysHeapRegionGetCpuPAddr(psPrivData->psTDFWCodePhysHeap,
	                                   ui32FWCodeRegionId,
	                                   &psPrivData->sCpuPAddr);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Could not acquire secure region CPU address", __func__));
		goto errorOnValidateParams;
	}

	if ((((1ULL << uiLog2Align) - 1) & psPrivData->sCpuPAddr.uiAddr) != 0)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "Trusted Device physical heap has the wrong alignment! "
				 "Physical address 0x%llx, alignment mask 0x%llx",
				 (unsigned long long) psPrivData->sCpuPAddr.uiAddr,
				 ((1ULL << uiLog2Align) - 1)));
		eError = PVRSRV_ERROR_REQUEST_TDFWCODE_PAGES_FAIL;
		goto errorOnValidateParams;
	}

	eError = PhysHeapRegionGetSize(psPrivData->psTDFWCodePhysHeap,
	                               ui32FWCodeRegionId,
	                               &psPrivData->ui64Size);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Could not acquire secure region size", __func__));
		goto errorOnValidateParams;
	}

	if (uiSize > psPrivData->ui64Size)
	{
		PVR_DPF((PVR_DBG_ERROR, "Trusted Device physical heap not big enough! Required %llu, available %llu",
				 uiSize, psPrivData->ui64Size));
		eError = PVRSRV_ERROR_REQUEST_TDFWCODE_PAGES_FAIL;
		goto errorOnValidateParams;
	}

	PhysHeapCpuPAddrToDevPAddr(psPrivData->psTDFWCodePhysHeap,
	                           1,
	                           &psPrivData->sDevPAddr,
	                           &psPrivData->sCpuPAddr);

	/* Check that the FW code is aligned to a Rogue cache line */
	if (ui32CacheLineSize > 0 &&
		(psPrivData->sDevPAddr.uiAddr & (ui32CacheLineSize - 1)) != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "Trusted Device physical heap not aligned to a Rogue cache line!"));
		eError = PVRSRV_ERROR_REQUEST_TDFWCODE_PAGES_FAIL;
		goto errorOnValidateParams;
	}

	/*
	 * uiLog2Align is only used to check the alignment of the secure memory
	 * region. The page size is still determined by the OS, and we expect the
	 * number of pages from TDGetTDFWCodeParams to have the same granularity.
	 */
	psPrivData->uiLog2Align = OSGetPageShift();

	eError = PMRCreatePMR(psDevNode,
	                      psPrivData->psTDFWCodePhysHeap,
	                      psPrivData->ui64Size,
	                      psPrivData->ui64Size,
	                      1,                 /* ui32NumPhysChunks */
	                      1,                 /* ui32NumVirtChunks */
	                      &uiMappingTable,   /* pui32MappingTable (not used) */
	                      uiLog2Align,       /* uiLog2ContiguityGuarantee */
	                      uiPMRFlags,
	                      eRegion == PVRSRV_DEVICE_FW_PRIVATE_DATA_REGION ?
	                      "TDFWDATA_PMR" : "TDFWCODE_PMR",
	                      &_sPMRTDFWCodeFuncTab,
	                      psPrivData,
	                      PMR_TYPE_TDFWCODE,
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
errorOnValidateParams:
	PhysHeapRelease(psPrivData->psTDFWCodePhysHeap);

errorOnAcquireHeap:
	OSFreeMem(psPrivData);

errorOnAllocData:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

#else /* !defined(NO_HARDWARE) */

#include "physmem_osmem.h"

typedef struct _PMR_TDFWCODE_DATA_ {
	PHYS_HEAP  *psTDFWCodePhysHeap;
	PMR        *psOSMemPMR;
	IMG_UINT32 ui32Log2PageSize;
} PMR_TDFWCODE_DATA;


/*
 * Implementation of callback functions
 */

static PVRSRV_ERROR
PMRLockPhysAddressesTDFWCodeMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_TDFWCODE_DATA *psPrivData = pvPriv;

	return PMRLockSysPhysAddresses(psPrivData->psOSMemPMR);
}

static PVRSRV_ERROR
PMRUnlockPhysAddressesTDFWCodeMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_TDFWCODE_DATA *psPrivData = pvPriv;

	return PMRUnlockSysPhysAddresses(psPrivData->psOSMemPMR);
}

static PVRSRV_ERROR
PMRSysPhysAddrTDFWCodeMem(PMR_IMPL_PRIVDATA pvPriv,
                          IMG_UINT32 ui32Log2PageSize,
                          IMG_UINT32 ui32NumOfPages,
                          IMG_DEVMEM_OFFSET_T *puiOffset,
                          IMG_BOOL *pbValid,
                          IMG_DEV_PHYADDR *psDevPAddr)
{
	PMR_TDFWCODE_DATA *psPrivData = pvPriv;

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
PMRAcquireKernelMappingDataTDFWCodeMem(PMR_IMPL_PRIVDATA pvPriv,
                                       size_t uiOffset,
                                       size_t uiSize,
                                       void **ppvKernelAddressOut,
                                       IMG_HANDLE *phHandleOut,
                                       PMR_FLAGS_T ulFlags)
{
	PMR_TDFWCODE_DATA *psPrivData = pvPriv;
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
PMRReleaseKernelMappingDataTDFWCodeMem(PMR_IMPL_PRIVDATA pvPriv,
                                       IMG_HANDLE hHandle)
{
	PMR_TDFWCODE_DATA *psPrivData = pvPriv;

	PMRReleaseKernelMappingData(psPrivData->psOSMemPMR, hHandle);
}

static PVRSRV_ERROR PMRFinalizeTDFWCodeMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_TDFWCODE_DATA *psPrivData = pvPriv;

	PMRUnrefPMR(psPrivData->psOSMemPMR);
	PhysHeapRelease(psPrivData->psTDFWCodePhysHeap);
	OSFreeMem(psPrivData);

	return PVRSRV_OK;
}

static PMR_IMPL_FUNCTAB _sPMRTDFWCodeFuncTab = {
	.pfnLockPhysAddresses        = &PMRLockPhysAddressesTDFWCodeMem,
	.pfnUnlockPhysAddresses      = &PMRUnlockPhysAddressesTDFWCodeMem,
	.pfnDevPhysAddr              = &PMRSysPhysAddrTDFWCodeMem,
	.pfnAcquireKernelMappingData = &PMRAcquireKernelMappingDataTDFWCodeMem,
	.pfnReleaseKernelMappingData = &PMRReleaseKernelMappingDataTDFWCodeMem,
	.pfnFinalize                 = &PMRFinalizeTDFWCodeMem,
};


/*
 * Public functions
 */
PVRSRV_ERROR PhysmemNewTDFWCodePMR(PVRSRV_DEVICE_NODE *psDevNode,
                                   IMG_DEVMEM_SIZE_T uiSize,
                                   PMR_LOG2ALIGN_T uiLog2Align,
                                   PVRSRV_MEMALLOCFLAGS_T uiFlags,
                                   PVRSRV_TD_FW_MEM_REGION eRegion,
                                   PMR **ppsPMRPtr)
{
	RGX_DATA *psRGXData = (RGX_DATA *)(psDevNode->psDevConfig->hDevData);
	PMR_TDFWCODE_DATA *psPrivData = NULL;
	PMR *psPMR = NULL;
	PMR *psOSPMR = NULL;
	IMG_UINT32 uiMappingTable = 0;
	PMR_FLAGS_T uiPMRFlags;
	PVRSRV_ERROR eError;

	/* In this instance, we simply pass flags straight through.
	 * Generically, uiFlags can include things that control the PMR
	 * factory, but we don't need any such thing (at the time of
	 * writing!), and our caller specifies all PMR flags so we don't
	 * need to meddle with what was given to us.
	 */
	uiPMRFlags = (PMR_FLAGS_T)(uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK);

	/* Check no significant bits were lost in cast due to different bit widths for flags */
	PVR_ASSERT(uiPMRFlags == (uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK));

	psPrivData = OSAllocZMem(sizeof(PMR_TDFWCODE_DATA));
	if (psPrivData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto errorOnAllocData;
	}

	/* Get required info for the TD Meta Code physical heap */
	if (!psRGXData->bHasTDFWCodePhysHeap)
	{
		PVR_DPF((PVR_DBG_ERROR, "Trusted Device physical heap not available!"));
		eError = PVRSRV_ERROR_REQUEST_TDFWCODE_PAGES_FAIL;
		goto errorOnAcquireHeap;
	}
	eError = PhysHeapAcquire(psRGXData->uiTDFWCodePhysHeapID,
	                         &psPrivData->psTDFWCodePhysHeap);
	if (eError != PVRSRV_OK) goto errorOnAcquireHeap;

	/*
	 * The alignment requested by the caller is only used to generate the
	 * secure FW allocation pdump command with the correct alignment.
	 * Internally we use another PMR with OS page alignment.
	 */
	psPrivData->ui32Log2PageSize = OSGetPageShift();

	eError = PhysmemNewOSRamBackedPMR(psDevNode,
	                                  uiSize,
	                                  uiSize,
	                                  1,                 /* ui32NumPhysChunks */
	                                  1,                 /* ui32NumVirtChunks */
	                                  &uiMappingTable,
	                                  psPrivData->ui32Log2PageSize,
	                                  uiFlags,
	                                  eRegion == PVRSRV_DEVICE_FW_PRIVATE_DATA_REGION ?
	                                  "TDFWDATA_OSMEM" : "TDFWCODE_OSMEM",
	                                  OSGetCurrentClientProcessIDKM(),
	                                  &psOSPMR);
	if (eError != PVRSRV_OK)
	{
		goto errorOnCreateOSPMR;
	}

	/* This is the primary PMR dumped with correct memspace and alignment */
	eError = PMRCreatePMR(psDevNode,
	                      psPrivData->psTDFWCodePhysHeap,
	                      uiSize,
	                      uiSize,
	                      1,                 /* ui32NumPhysChunks */
	                      1,                 /* ui32NumVirtChunks */
	                      &uiMappingTable,   /* pui32MappingTable (not used) */
	                      uiLog2Align,       /* uiLog2ContiguityGuarantee */
	                      uiPMRFlags,
	                      eRegion == PVRSRV_DEVICE_FW_PRIVATE_DATA_REGION ?
	                      "TDFWDATA_PMR" : "TDFWCODE_PMR",
	                      &_sPMRTDFWCodeFuncTab,
	                      psPrivData,
	                      PMR_TYPE_TDFWCODE,
	                      &psPMR,
	                      PDUMP_NONE);
	if (eError != PVRSRV_OK)
	{
		goto errorOnCreateTDPMR;
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

errorOnCreateTDPMR:
	PMRUnrefPMR(psOSPMR);

errorOnCreateOSPMR:
	PhysHeapRelease(psPrivData->psTDFWCodePhysHeap);

errorOnAcquireHeap:
	OSFreeMem(psPrivData);

errorOnAllocData:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}
#endif /* !defined(NO_HARDWARE) */

