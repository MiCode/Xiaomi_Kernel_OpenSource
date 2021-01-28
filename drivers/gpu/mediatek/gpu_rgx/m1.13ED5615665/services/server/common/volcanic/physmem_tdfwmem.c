/*************************************************************************/ /*!
@File
@Title          PMR functions for Trusted Device firmware memory
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
#include "physmem_tdfwmem.h"
#include "physheap.h"
#include "rgxdevice.h"

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
#include "ri_server.h"
#endif


#if !defined(SUPPORT_SECURITY_VALIDATION)

typedef struct _PMR_TDFWMEM_DATA_ {
	PHYS_HEAP       *psTDFWMemPhysHeap;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_UINT64      ui64Size;
	IMG_UINT32      ui32Log2PageSize;
} PMR_TDFWMEM_DATA;


/*
 * Implementation of callback functions
 */

static PVRSRV_ERROR PMRSysPhysAddrTDFWMem(PMR_IMPL_PRIVDATA pvPriv,
                                          IMG_UINT32 ui32Log2PageSize,
                                          IMG_UINT32 ui32NumOfPages,
                                          IMG_DEVMEM_OFFSET_T *puiOffset,
                                          IMG_BOOL *pbValid,
                                          IMG_DEV_PHYADDR *psDevPAddr)
{
	PMR_TDFWMEM_DATA *psPrivData = pvPriv;
	IMG_UINT32 i;

	if (psPrivData->ui32Log2PageSize != ui32Log2PageSize)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Incompatible contiguity (requested %u, got %u)",
				 __func__, ui32Log2PageSize, psPrivData->ui32Log2PageSize));
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

static PVRSRV_ERROR PMRFinalizeTDFWMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_TDFWMEM_DATA *psPrivData = NULL;

	psPrivData = pvPriv;
	PhysHeapRelease(psPrivData->psTDFWMemPhysHeap);
	OSFreeMem(psPrivData);

	return PVRSRV_OK;
}

static PMR_IMPL_FUNCTAB _sPMRTDFWMemFuncTab = {
	.pfnDevPhysAddr = &PMRSysPhysAddrTDFWMem,
	.pfnFinalize    = &PMRFinalizeTDFWMem,
};


/*
 * Public functions
 */
PVRSRV_ERROR PhysmemNewTDFWMemPMR(CONNECTION_DATA *psConnection,
                                  PVRSRV_DEVICE_NODE *psDevNode,
                                  IMG_DEVMEM_SIZE_T uiSize,
                                  PMR_LOG2ALIGN_T uiLog2Align,
                                  PVRSRV_MEMALLOCFLAGS_T uiFlags,
                                  PVRSRV_TD_FW_MEM_REGION eRegion,
                                  PMR **ppsPMRPtr)
{
	PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;
	RGX_DATA *psRGXData = (RGX_DATA *)(psDevConfig->hDevData);
	PMR_TDFWMEM_DATA *psPrivData = NULL;
	PMR *psPMR = NULL;
	IMG_UINT32 uiMappingTable = 0;
	PMR_FLAGS_T uiPMRFlags;
	IMG_UINT32 ui32CacheLineSize = 0;
	PVRSRV_ERROR eError;

	PVR_UNREFERENCED_PARAMETER(psConnection);

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

	psPrivData = OSAllocZMem(sizeof(PMR_TDFWMEM_DATA));
	PVR_GOTO_IF_NOMEM(psPrivData, eError, errorOnAllocData);

	/* Get required info for the TD Meta Code physical heap */
	if (!psRGXData->bHasTDFWMemPhysHeap)
	{
		PVR_LOG_GOTO_WITH_ERROR("psRGXData->bHasTDFWMemPhysHeap",
		                        eError, PVRSRV_ERROR_REQUEST_TDFWMEM_PAGES_FAIL, errorOnAcquireHeap);
	}

	eError = PhysHeapAcquire(psRGXData->uiTDFWMemPhysHeapID,
	                         &psPrivData->psTDFWMemPhysHeap);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Could not acquire secure physical heap %u",
				__func__, psRGXData->uiTDFWMemPhysHeapID));
		goto errorOnAcquireHeap;
	}

	eError = PhysHeapRegionGetCpuPAddr(psPrivData->psTDFWMemPhysHeap,
	                                   eRegion,
	                                   &psPrivData->sCpuPAddr);
	PVR_LOG_GOTO_IF_ERROR(eError, "PhysHeapRegionGetCpuPAddr", errorOnValidateParams);

	if ((((1ULL << uiLog2Align) - 1) & psPrivData->sCpuPAddr.uiAddr) != 0)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "Trusted Device physical heap has the wrong alignment! "
				 "Physical address 0x%llx, alignment mask 0x%llx",
				 (unsigned long long) psPrivData->sCpuPAddr.uiAddr,
				 ((1ULL << uiLog2Align) - 1)));
		PVR_GOTO_WITH_ERROR(eError, PVRSRV_ERROR_REQUEST_TDFWMEM_PAGES_FAIL, errorOnValidateParams);
	}

	eError = PhysHeapRegionGetSize(psPrivData->psTDFWMemPhysHeap,
	                               eRegion,
	                               &psPrivData->ui64Size);
	PVR_LOG_GOTO_IF_ERROR(eError, "PhysHeapRegionGetSize", errorOnValidateParams);

	if (uiSize > psPrivData->ui64Size)
	{
		PVR_DPF((PVR_DBG_ERROR, "Trusted Device physical heap not big enough! Required %llu, available %llu",
				 uiSize, psPrivData->ui64Size));
		PVR_GOTO_WITH_ERROR(eError, PVRSRV_ERROR_REQUEST_TDFWMEM_PAGES_FAIL, errorOnValidateParams);
	}

	PhysHeapCpuPAddrToDevPAddr(psPrivData->psTDFWMemPhysHeap,
	                           1,
	                           &psPrivData->sDevPAddr,
	                           &psPrivData->sCpuPAddr);

	/* Check that the FW memory is aligned to a Rogue cache line */
	if (ui32CacheLineSize > 0 &&
		(psPrivData->sDevPAddr.uiAddr & (ui32CacheLineSize - 1)) != 0)
	{
		PVR_LOG_GOTO_WITH_ERROR("Trusted Device physical heap not aligned to a Rogue cache line",
		                        eError, PVRSRV_ERROR_REQUEST_TDFWMEM_PAGES_FAIL, errorOnValidateParams);
	}

	/*
	 * ui32Log2PageSize is only used to check the alignment of the secure memory
	 * region. The page size is still determined by the OS, and we expect the
	 * number of pages from TDGetTDFWCodeParams to have the same granularity.
	 */
	psPrivData->ui32Log2PageSize = OSGetPageShift();

	eError = PMRCreatePMR(psDevNode,
	                      psPrivData->psTDFWMemPhysHeap,
	                      psPrivData->ui64Size,
	                      psPrivData->ui64Size,
	                      1,                 /* ui32NumPhysChunks */
	                      1,                 /* ui32NumVirtChunks */
	                      &uiMappingTable,   /* pui32MappingTable (not used) */
	                      uiLog2Align,       /* uiLog2ContiguityGuarantee */
	                      uiPMRFlags,
	                      "TDFWMEM_PMR",
	                      &_sPMRTDFWMemFuncTab,
	                      psPrivData,
	                      PMR_TYPE_TDFWMEM,
	                      &psPMR,
	                      PDUMP_NONE);
	PVR_GOTO_IF_ERROR(eError, errorOnCreatePMR);

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
	eError = RIWritePMREntryKM(psPMR);
	PVR_WARN_IF_ERROR(eError, "RIWritePMREntryKM");
#endif

	*ppsPMRPtr = psPMR;
	return PVRSRV_OK;

errorOnCreatePMR:
errorOnValidateParams:
	PhysHeapRelease(psPrivData->psTDFWMemPhysHeap);

errorOnAcquireHeap:
	OSFreeMem(psPrivData);

errorOnAllocData:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

#else /* !defined(SUPPORT_SECURITY_VALIDATION) */

#include "physmem.h"

/*
 * Public functions
 */
PVRSRV_ERROR PhysmemNewTDFWMemPMR(CONNECTION_DATA *psConnection,
                                  PVRSRV_DEVICE_NODE *psDevNode,
                                  IMG_DEVMEM_SIZE_T uiSize,
                                  PMR_LOG2ALIGN_T uiLog2Align,
                                  PVRSRV_MEMALLOCFLAGS_T uiFlags,
                                  PVRSRV_TD_FW_MEM_REGION eRegion,
                                  PMR **ppsPMRPtr)
{
	PMR *psPMR = NULL;
	IMG_UINT32 uiLog2AllocPageSize = OSGetPageShift();
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

	/* Add validation flag */
	if (eRegion == PVRSRV_DEVICE_FW_CODE_REGION || eRegion == PVRSRV_DEVICE_FW_COREMEM_CODE_REGION)
	{
		uiPMRFlags |= PVRSRV_MEMALLOCFLAG_VAL_SECURE_FW_CODE;
	}
	else if (eRegion == PVRSRV_DEVICE_FW_PRIVATE_DATA_REGION || eRegion == PVRSRV_DEVICE_FW_COREMEM_DATA_REGION)
	{
		uiPMRFlags |= PVRSRV_MEMALLOCFLAG_VAL_SECURE_FW_DATA;
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Invalid TD FW memory region %u", __func__, eRegion));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto errorOnCreatePMR;
	}

	/* Align allocation size to page size */
	uiSize = PVR_ALIGN(uiSize, 1 << uiLog2AllocPageSize);

	eError = PhysmemNewRamBackedPMR(psConnection,
	                                psDevNode,
	                                uiSize,
	                                uiSize,
	                                1,                 /* ui32NumPhysChunks */
	                                1,                 /* ui32NumVirtChunks */
	                                &uiMappingTable,
	                                uiLog2AllocPageSize,
	                                uiPMRFlags,
	                                strlen("TDFWMEM") + 1,
	                                "TDFWMEM",
	                                OSGetCurrentClientProcessIDKM(),
	                                &psPMR,
	                                PDUMP_NONE);
	PVR_GOTO_IF_ERROR(eError, errorOnCreatePMR);

	/* All the PMR callbacks will be redirected to the internal LMA PMR */

	*ppsPMRPtr = psPMR;
	return PVRSRV_OK;

errorOnCreatePMR:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

#endif /* !defined(SUPPORT_SECURITY_VALIDATION) */
