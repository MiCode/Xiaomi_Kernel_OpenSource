/*************************************************************************/ /*!
@File           physmem.c
@Title          Physmem
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Common entry point for creation of RAM backed PMR's
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
*/ /***************************************************************************/
#include "img_types.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"
#include "device.h"
#include "physmem.h"
#include "pvrsrv.h"
#include "osfunc.h"
#include "pdump_physmem.h"
#include "pdump_km.h"
#include "rgx_heaps.h"

#if defined(DEBUG)
static IMG_UINT32 gPMRAllocFail;

#if defined(LINUX)
#include <linux/moduleparam.h>

module_param(gPMRAllocFail, uint, 0644);
MODULE_PARM_DESC(gPMRAllocFail, "When number of PMR allocs reaches "
				 "this value, it will fail (default value is 0 which "
				 "means that alloc function will behave normally).");
#endif /* defined(LINUX) */
#endif /* defined(DEBUG) */

PVRSRV_ERROR DevPhysMemAlloc(PVRSRV_DEVICE_NODE	*psDevNode,
                             IMG_UINT32 ui32MemSize,
                             IMG_UINT32 ui32Log2Align,
                             const IMG_UINT8 u8Value,
                             IMG_BOOL bInitPage,
#if defined(PDUMP)
                             const IMG_CHAR *pszDevSpace,
                             const IMG_CHAR *pszSymbolicAddress,
                             IMG_HANDLE *phHandlePtr,
#endif
                             IMG_HANDLE hMemHandle,
                             IMG_DEV_PHYADDR *psDevPhysAddr)
{
	void *pvCpuVAddr;
	PVRSRV_ERROR eError;
#if defined(PDUMP)
	IMG_CHAR szFilenameOut[PDUMP_PARAM_MAX_FILE_NAME];
	PDUMP_FILEOFFSET_T uiOffsetOut;
	IMG_UINT32 ui32PageSize;
	IMG_UINT32 ui32PDumpMemSize = ui32MemSize;
#endif
	PG_HANDLE *psMemHandle;
	IMG_UINT64 uiMask;
	IMG_DEV_PHYADDR sDevPhysAddr_int;

	psMemHandle = hMemHandle;

	/* Allocate the pages */
	eError = psDevNode->pfnDevPxAlloc(psDevNode,
	                                  TRUNCATE_64BITS_TO_SIZE_T(ui32MemSize),
	                                  psMemHandle,
	                                  &sDevPhysAddr_int);
	if (PVRSRV_OK != eError)
	{
		PVR_DPF((PVR_DBG_ERROR,"Unable to allocate the pages"));
		return eError;
	}

	/* Check to see if the page allocator returned pages with our desired
	 * alignment, which is not unlikely
	 */
	uiMask = (1 << ui32Log2Align) - 1;
	if (ui32Log2Align && (sDevPhysAddr_int.uiAddr & uiMask))
	{
		/* use over allocation instead */
		psDevNode->pfnDevPxFree(psDevNode, psMemHandle);

		ui32MemSize += (IMG_UINT32) uiMask;
		eError = psDevNode->pfnDevPxAlloc(psDevNode,
		                                  TRUNCATE_64BITS_TO_SIZE_T(ui32MemSize),
		                                  psMemHandle,
		                                  &sDevPhysAddr_int);
		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_ERROR,"Unable to over-allocate the pages"));
			return eError;
		}

		sDevPhysAddr_int.uiAddr += uiMask;
		sDevPhysAddr_int.uiAddr &= ~uiMask;
	}
	*psDevPhysAddr = sDevPhysAddr_int;

#if defined(PDUMP)
	ui32PageSize = ui32Log2Align? (1 << ui32Log2Align) : OSGetPageSize();
	eError = PDumpMalloc(pszDevSpace,
								pszSymbolicAddress,
								ui32PDumpMemSize,
								ui32PageSize,
								IMG_FALSE,
								0,
								phHandlePtr,
								PDUMP_NONE);
	if (PVRSRV_OK != eError)
	{
		PDUMPCOMMENT("Allocating pages failed");
		*phHandlePtr = NULL;
	}
#endif

	if (bInitPage)
	{
		/*Map the page to the CPU VA space */
		eError = psDevNode->pfnDevPxMap(psDevNode,
		                                psMemHandle,
		                                ui32MemSize,
		                                &sDevPhysAddr_int,
		                                &pvCpuVAddr);
		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_ERROR,"Unable to map the allocated page"));
			psDevNode->pfnDevPxFree(psDevNode, psMemHandle);
			return eError;
		}

		/*Fill the memory with given content */
		OSDeviceMemSet(pvCpuVAddr, u8Value, ui32MemSize);

		/*Map the page to the CPU VA space */
		eError = psDevNode->pfnDevPxClean(psDevNode,
		                                  psMemHandle,
		                                  0,
		                                  ui32MemSize);
		if (PVRSRV_OK != eError)
		{
			PVR_DPF((PVR_DBG_ERROR,"Unable to clean the allocated page"));
			psDevNode->pfnDevPxUnMap(psDevNode, psMemHandle, pvCpuVAddr);
			psDevNode->pfnDevPxFree(psDevNode, psMemHandle);
			return eError;
		}

#if defined(PDUMP)
		/*P-Dumping of the page contents can be done in two ways
		 * 1. Store the single byte init value to the .prm file
		 *    and load the same value to the entire dummy page buffer
		 *    This method requires lot of LDB's inserted into the out2.txt
		 *
		 * 2. Store the entire contents of the buffer to the .prm file
		 *    and load them back.
		 *    This only needs a single LDB instruction in the .prm file
		 *    and chosen this method
		 *    size of .prm file might go up but that's not huge at least
		 *    for this allocation
		 */
		/*Write the buffer contents to the prm file */
		eError = PDumpWriteBuffer(pvCpuVAddr,
		                          ui32PDumpMemSize,
		                          PDUMP_FLAGS_CONTINUOUS,
		                          szFilenameOut,
		                          sizeof(szFilenameOut),
		                          &uiOffsetOut);
		if (PVRSRV_OK == eError)
		{
			/* Load the buffer back to the allocated memory when playing the pdump */
			eError = PDumpPMRLDB(pszDevSpace,
			                     pszSymbolicAddress,
			                     0,
			                     ui32PDumpMemSize,
			                     szFilenameOut,
			                     uiOffsetOut,
			                     PDUMP_FLAGS_CONTINUOUS);
			if (PVRSRV_OK != eError)
			{
				PDUMP_ERROR(eError, "Failed to write LDB statement to script file");
				PVR_DPF((PVR_DBG_ERROR, "Failed to write LDB statement to script file, error %d", eError));
			}

		}
		else if (eError != PVRSRV_ERROR_PDUMP_NOT_ALLOWED)
		{
			PDUMP_ERROR(eError, "Failed to write device allocation to parameter file");
			PVR_DPF((PVR_DBG_ERROR, "Failed to write device allocation to parameter file, error %d", eError));
		}
		else
		{
			/* else Write to parameter file prevented under the flags and
			 * current state of the driver so skip write to script and error IF.
			 */
			eError = PVRSRV_OK;
		}
#endif

		/*UnMap the page */
		psDevNode->pfnDevPxUnMap(psDevNode,
		                         psMemHandle,
		                         pvCpuVAddr);
	}

	return PVRSRV_OK;
}

void DevPhysMemFree(PVRSRV_DEVICE_NODE *psDevNode,
#if defined(PDUMP)
							IMG_HANDLE hPDUMPMemHandle,
#endif
							IMG_HANDLE	hMemHandle)
{
	PG_HANDLE *psMemHandle;

	psMemHandle = hMemHandle;
	psDevNode->pfnDevPxFree(psDevNode, psMemHandle);
#if defined(PDUMP)
	if (NULL != hPDUMPMemHandle)
	{
		PDumpFree(hPDUMPMemHandle);
	}
#endif

}


/* Checks the input parameters and adjusts them if possible and necessary */
static inline PVRSRV_ERROR _ValidateParams(IMG_UINT32 ui32NumPhysChunks,
                                           IMG_UINT32 ui32NumVirtChunks,
                                           PVRSRV_MEMALLOCFLAGS_T uiFlags,
                                           IMG_UINT32 *puiLog2AllocPageSize,
                                           IMG_DEVMEM_SIZE_T *puiSize,
                                           PMR_SIZE_T *puiChunkSize)
{
	IMG_UINT32 uiLog2AllocPageSize = *puiLog2AllocPageSize;
	IMG_DEVMEM_SIZE_T uiSize = *puiSize;
	PMR_SIZE_T uiChunkSize = *puiChunkSize;
	/* Sparse if we have different number of virtual and physical chunks plus
	 * in general all allocations with more than one virtual chunk */
	IMG_BOOL bIsSparse = (ui32NumVirtChunks != ui32NumPhysChunks ||
			ui32NumVirtChunks > 1) ? IMG_TRUE : IMG_FALSE;

	/* Protect against ridiculous page sizes */
	if (uiLog2AllocPageSize > RGX_HEAP_2MB_PAGE_SHIFT)
	{
		PVR_DPF((PVR_DBG_ERROR, "Page size is too big: 2^%u.", uiLog2AllocPageSize));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Sanity check of the alloc size */
	if (uiSize >= 0x1000000000ULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Cancelling allocation request of over 64 GB. "
				 "This is likely a bug."
				, __func__));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Fail if requesting coherency on one side but uncached on the other */
	if ( (PVRSRV_CHECK_CPU_CACHE_COHERENT(uiFlags) &&
	         (PVRSRV_CHECK_GPU_UNCACHED(uiFlags) || PVRSRV_CHECK_GPU_WRITE_COMBINE(uiFlags))) )
	{
		PVR_DPF((PVR_DBG_ERROR, "Request for CPU coherency but specifying GPU uncached "
				"Please use GPU cached flags for coherency."));
		return PVRSRV_ERROR_UNSUPPORTED_CACHE_MODE;
	}

	if ( (PVRSRV_CHECK_GPU_CACHE_COHERENT(uiFlags) &&
	         (PVRSRV_CHECK_CPU_UNCACHED(uiFlags) || PVRSRV_CHECK_CPU_WRITE_COMBINE(uiFlags))) )
	{
		PVR_DPF((PVR_DBG_ERROR, "Request for GPU coherency but specifying CPU uncached "
				"Please use CPU cached flags for coherency."));
		return PVRSRV_ERROR_UNSUPPORTED_CACHE_MODE;
	}

	if (PVRSRV_CHECK_ZERO_ON_ALLOC(uiFlags) && PVRSRV_CHECK_POISON_ON_ALLOC(uiFlags))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Zero on Alloc and Poison on Alloc are mutually exclusive.",
				__func__));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (bIsSparse)
	{
		/* For sparse we need correct parameters like a suitable page size....  */
		if (OSGetPageShift() > uiLog2AllocPageSize)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Invalid log2-contiguity for sparse allocation. "
					"Requested %u, required minimum %zd",
					__func__,
					uiLog2AllocPageSize,
					OSGetPageShift() ));

			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		/* ... chunk size must be a equal to page size ...*/
		if ( uiChunkSize != (1 << uiLog2AllocPageSize) )
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Invalid chunk size for sparse allocation. "
					"Requested %#llx, must be same as page size %#x.",
					__func__,
					(long long unsigned) uiChunkSize,
					1 << uiLog2AllocPageSize ));

			return PVRSRV_ERROR_PMR_NOT_PAGE_MULTIPLE;
		}

		if (ui32NumVirtChunks * uiChunkSize != uiSize)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Total alloc size (%#llx) is not qual "
					"to virtual chunks * chunk size (%#llx)",
					__func__,
					(long long unsigned) uiSize,
					(long long unsigned) (ui32NumVirtChunks * uiChunkSize) ));

			return PVRSRV_ERROR_PMR_NOT_PAGE_MULTIPLE;
		}

		if (ui32NumPhysChunks > ui32NumVirtChunks)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Number of physical chunks (%u) must not be greater "
					"than number of virtual chunks (%u)",
					__func__,
					ui32NumPhysChunks,
					ui32NumVirtChunks));

			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}
	else
	{
		/*
		 * Silently round up alignment/pagesize if request was less that PAGE_SHIFT
		 * because it would never be harmful for memory to be _more_ contiguous that
		 * was desired.
		 */
		uiLog2AllocPageSize = OSGetPageShift() > uiLog2AllocPageSize ?
				OSGetPageShift() : uiLog2AllocPageSize;

		/* Same for total size */
		uiSize = PVR_ALIGN(uiSize, (IMG_DEVMEM_SIZE_T)OSGetPageSize());
		*puiChunkSize = uiSize;
	}

	if ((uiSize & ((1ULL << uiLog2AllocPageSize) - 1)) != 0)
	{
		PVR_DPF((PVR_DBG_ERROR,
							"%s: Total size (%#llx) must be a multiple "
							"of the requested contiguity (%u)",
							__func__,
							(long long unsigned) uiSize,
							(1 << uiLog2AllocPageSize)));
		return PVRSRV_ERROR_PMR_NOT_PAGE_MULTIPLE;
	}

	*puiLog2AllocPageSize = uiLog2AllocPageSize;
	*puiSize = uiSize;

	return PVRSRV_OK;
}

PVRSRV_ERROR
PhysmemNewRamBackedPMR(CONNECTION_DATA * psConnection,
                       PVRSRV_DEVICE_NODE *psDevNode,
                       IMG_DEVMEM_SIZE_T uiSize,
                       PMR_SIZE_T uiChunkSize,
                       IMG_UINT32 ui32NumPhysChunks,
                       IMG_UINT32 ui32NumVirtChunks,
                       IMG_UINT32 *pui32MappingTable,
                       IMG_UINT32 uiLog2AllocPageSize,
                       PVRSRV_MEMALLOCFLAGS_T uiFlags,
                       IMG_UINT32 uiAnnotationLength,
                       const IMG_CHAR *pszAnnotation,
                       IMG_PID uiPid,
                       PMR **ppsPMRPtr)
{
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_PHYS_HEAP ePhysHeapIdx;
	PFN_SYS_DEV_CHECK_MEM_ALLOC_SIZE pfnCheckMemAllocSize =
		psDevNode->psDevConfig->pfnCheckMemAllocSize;

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(uiAnnotationLength);

	eError = _ValidateParams(ui32NumPhysChunks,
	                         ui32NumVirtChunks,
	                         uiFlags,
	                         &uiLog2AllocPageSize,
	                         &uiSize,
	                         &uiChunkSize);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/* Lookup the requested physheap index to use for this PMR allocation */
	if (PVRSRV_CHECK_FW_LOCAL(uiFlags))
	{
		ePhysHeapIdx = PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL;
	}
	else if (PVRSRV_CHECK_CPU_LOCAL(uiFlags))
	{
		ePhysHeapIdx = PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL;
	}
	else
	{
		ePhysHeapIdx = PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL;
	}

	if (NULL == psDevNode->apsPhysHeap[ePhysHeapIdx])
	{
		/* In case a heap hasn't been acquired for this type, return invalid heap error */
		PVR_DPF((PVR_DBG_ERROR, "%s: Requested allocation on device node (%p) from "
		        "an invalid heap (HeapIndex=%d)",
		        __FUNCTION__, psDevNode, ePhysHeapIdx));
		return PVRSRV_ERROR_INVALID_HEAP;
	}

	/* Apply memory budgeting policy */
	if (pfnCheckMemAllocSize)
	{
		IMG_UINT64 uiMemSize = (IMG_UINT64)uiChunkSize * ui32NumPhysChunks;
		PVRSRV_ERROR eError;

		eError = pfnCheckMemAllocSize(psDevNode->psDevConfig->hSysData, uiMemSize);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}
	}

#if defined(DEBUG)
	if (gPMRAllocFail > 0)
	{
		static IMG_UINT32 ui32AllocCount = 1;

		if (ui32AllocCount < gPMRAllocFail)
		{
			ui32AllocCount++;
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR, "%s failed on %d allocation.",
			         __func__, ui32AllocCount));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
	}
#endif /* defined(DEBUG) */

	return psDevNode->pfnCreateRamBackedPMR[ePhysHeapIdx](psDevNode,
											uiSize,
											uiChunkSize,
											ui32NumPhysChunks,
											ui32NumVirtChunks,
											pui32MappingTable,
											uiLog2AllocPageSize,
											uiFlags,
											pszAnnotation,
											uiPid,
											ppsPMRPtr);
}

PVRSRV_ERROR
PhysmemNewRamBackedLockedPMR(CONNECTION_DATA * psConnection,
							PVRSRV_DEVICE_NODE *psDevNode,
							IMG_DEVMEM_SIZE_T uiSize,
							PMR_SIZE_T uiChunkSize,
							IMG_UINT32 ui32NumPhysChunks,
							IMG_UINT32 ui32NumVirtChunks,
							IMG_UINT32 *pui32MappingTable,
							IMG_UINT32 uiLog2PageSize,
							PVRSRV_MEMALLOCFLAGS_T uiFlags,
							IMG_UINT32 uiAnnotationLength,
							const IMG_CHAR *pszAnnotation,
							IMG_PID uiPid,
							PMR **ppsPMRPtr)
{

	PVRSRV_ERROR eError;
	eError = PhysmemNewRamBackedPMR(psConnection,
									psDevNode,
									uiSize,
									uiChunkSize,
									ui32NumPhysChunks,
									ui32NumVirtChunks,
									pui32MappingTable,
									uiLog2PageSize,
									uiFlags,
									uiAnnotationLength,
									pszAnnotation,
									uiPid,
									ppsPMRPtr);

	if (eError == PVRSRV_OK)
	{
		eError = PMRLockSysPhysAddresses(*ppsPMRPtr);
	}

	return eError;
}

static void GetLMASize( IMG_DEVMEM_SIZE_T *puiLMASize,
			PVRSRV_DEVICE_NODE *psDevNode )
{
	IMG_UINT uiRegionIndex = 0, uiNumRegions = 0;
	PVR_ASSERT(psDevNode);

	uiNumRegions = psDevNode->psDevConfig->pasPhysHeaps[0].ui32NumOfRegions;

	for (uiRegionIndex = 0; uiRegionIndex < uiNumRegions; ++uiRegionIndex)
	{
		*puiLMASize += psDevNode->psDevConfig->pasPhysHeaps[0].pasRegions[uiRegionIndex].uiSize;
	}
}

PVRSRV_ERROR
PVRSRVGetMaxDevMemSizeKM( CONNECTION_DATA * psConnection,
			  PVRSRV_DEVICE_NODE *psDevNode,
			  IMG_DEVMEM_SIZE_T *puiLMASize,
			  IMG_DEVMEM_SIZE_T *puiUMASize )
{
	IMG_BOOL bLMA = IMG_FALSE, bUMA = IMG_FALSE;

	*puiLMASize = 0;
	*puiUMASize = 0;

#if defined(TC_MEMORY_CONFIG)			/* For TC2 */
#if (TC_MEMORY_CONFIG == TC_MEMORY_LOCAL)
	bLMA = IMG_TRUE;
#elif (TC_MEMORY_CONFIG == TC_MEMORY_HOST)
	bUMA = IMG_TRUE;
#else
	bUMA = IMG_TRUE;
	bLMA = IMG_TRUE;
#endif

#elif defined(PLATO_MEMORY_CONFIG)		/* For Plato TC */
#if (PLATO_MEMORY_CONFIG == PLATO_MEMORY_LOCAL)
	bLMA = IMG_TRUE;
#elif (PLATO_MEMORY_CONFIG == PLATO_MEMORY_HOST)
	bUMA = IMG_TRUE;
#else
	bUMA = IMG_TRUE;
	bLMA = IMG_TRUE;
#endif

#elif defined(LMA)				/* For emu, vp_linux */
	bLMA = IMG_TRUE;

#else						/* For all other platforms */
	bUMA = IMG_TRUE;
#endif

	if (bLMA) { GetLMASize(puiLMASize, psDevNode); }
	if (bUMA) { *puiUMASize = OSGetRAMSize(); }

	PVR_UNREFERENCED_PARAMETER(psConnection);
	return PVRSRV_OK;
}

/* 'Wrapper' function to call PMRImportPMR(), which
 * first checks the PMR is for the current device.
 * This avoids the need to do this in pmr.c, which
 * would then need PVRSRV_DEVICE_NODE (defining this
 * type in pmr.h causes a typedef redefinition issue).
 */
PVRSRV_ERROR
PhysmemImportPMR(CONNECTION_DATA *psConnection,
             PVRSRV_DEVICE_NODE *psDevNode,
             PMR_EXPORT *psPMRExport,
             PMR_PASSWORD_T uiPassword,
             PMR_SIZE_T uiSize,
             PMR_LOG2ALIGN_T uiLog2Contig,
             PMR **ppsPMR)
{
	PVR_UNREFERENCED_PARAMETER(psConnection);

	if (PMRGetExportDeviceNode(psPMRExport) != psDevNode)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: PMR invalid for this device\n", __func__));
		return PVRSRV_ERROR_PMR_NOT_PERMITTED;
	}

	return PMRImportPMR(psPMRExport,
	                    uiPassword,
	                    uiSize,
	                    uiLog2Contig,
	                    ppsPMR);
}
