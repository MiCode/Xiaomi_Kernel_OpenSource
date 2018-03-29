/*************************************************************************/ /*!
@File
@Title          Linux OS PMR functions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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

#include <asm/io.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#if defined(CONFIG_L4_LINUX)
#include <asm/api-l4env/api.h>
#endif
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
#include <linux/pfn_t.h>
#include <linux/pfn.h>
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */

#include "img_defs.h"
#include "pvr_debug.h"
#include "allocmem.h"
#include "devicemem_server_utils.h"
#include "pmr.h"
#include "pmr_os.h"

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif

#include "kernel_compatibility.h"

/*
 * x86_32:
 * Use vm_insert_page because remap_pfn_range has issues when mapping HIGHMEM
 * pages with default memory attributes; these HIGHMEM pages are skipped in
 * set_pages_array_[uc,wc] during allocation; see reserve_pfn_range().
 * Also vm_insert_page is faster.
 *
 * x86_64:
 * Use vm_insert_page because it is faster.
 *
 * Other platforms:
 * Use remap_pfn_range by default because it does not issue a cache flush.
 * It is known that ARM32 benefits from this. When other platforms become
 * available it has to be investigated if this assumption holds for them as well.
 *
 * Since vm_insert_page does more precise memory accounting we have the build
 * flag PVR_MMAP_USE_VM_INSERT that forces its use. This is useful as a debug
 * feature.
 *
 */
#if defined(CONFIG_X86) || defined(PVR_MMAP_USE_VM_INSERT)
#define PMR_OS_USE_VM_INSERT_PAGE 1
#endif

static void MMapPMROpen(struct vm_area_struct *ps_vma)
{
	PMR *psPMR = ps_vma->vm_private_data;

	/* Our VM flags should ensure this function never gets called */
	PVR_DPF((PVR_DBG_WARNING,
			 "%s: Unexpected mmap open call, this is probably an application bug.",
			 __func__));
	PVR_DPF((PVR_DBG_WARNING,
			 "%s: vma struct: 0x%p, vAddr: %#lX, length: %#lX, PMR pointer: 0x%p",
			 __func__,
			 ps_vma,
			 ps_vma->vm_start,
			 ps_vma->vm_end - ps_vma->vm_start,
			 psPMR));

	/* In case we get called anyway let's do things right by increasing the refcount and
	 * locking down the physical addresses. */
	PMRRefPMR(psPMR);

	if (PMRLockSysPhysAddresses(psPMR) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Could not lock down physical addresses, aborting.", __func__));
		PMRUnrefPMR(psPMR);
	}
}

static void MMapPMRClose(struct vm_area_struct *ps_vma)
{
	PMR *psPMR = ps_vma->vm_private_data;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if	defined(PVRSRV_ENABLE_MEMORY_STATS)
	{
		uintptr_t vAddr = ps_vma->vm_start;

		while (vAddr < ps_vma->vm_end)
		{
			/* USER MAPPING */
			PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_MAP_UMA_LMA_PAGES, (IMG_UINT64)vAddr);
			vAddr += PAGE_SIZE;
		}
	}
#else
	PVRSRVStatsDecrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_MAP_UMA_LMA_PAGES, ps_vma->vm_end - ps_vma->vm_start);
#endif
#endif

	PMRUnlockSysPhysAddresses(psPMR);
	PMRUnrefPMR(psPMR);
}

/*
 * This vma operation is used to read data from mmap regions. It is called
 * by access_process_vm, which is called to handle PTRACE_PEEKDATA ptrace
 * requests and reads from /proc/<pid>/mem.
 */
static int MMapVAccess(struct vm_area_struct *ps_vma, unsigned long addr,
		       void *buf, int len, int write)
{
	PMR *psPMR = ps_vma->vm_private_data;
	unsigned long ulOffset = addr - ps_vma->vm_start;
	size_t uiBytesCopied;
	PVRSRV_ERROR eError;
	int iRetVal = -EINVAL;

	if (write)
	{
		eError = PMR_WriteBytes(psPMR,
					(IMG_DEVMEM_OFFSET_T) ulOffset,
					buf,
					len,
					&uiBytesCopied);
	}
	else
	{
		eError = PMR_ReadBytes(psPMR,
				       (IMG_DEVMEM_OFFSET_T) ulOffset,
				       buf,
				       len,
				       &uiBytesCopied);
	}

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Error from %s (%d)",
			 __func__,
			 write ? "PMR_WriteBytes" : "PMR_ReadBytes",
			 eError));
	}
	else
	{
		iRetVal = uiBytesCopied;
	}

	return iRetVal;
}

static const struct vm_operations_struct gsMMapOps =
{
	.open = &MMapPMROpen,
	.close = &MMapPMRClose,
	.access = MMapVAccess,
};

PVRSRV_ERROR
OSMMapPMRGeneric(PMR *psPMR, PMR_MMAP_DATA pOSMMapData)
{
	struct vm_area_struct *ps_vma = pOSMMapData;
	PVRSRV_DEVICE_NODE *psDevNode = PMR_DeviceNode(psPMR);
	PVRSRV_ERROR eError;
	size_t uiLength;
	IMG_DEVMEM_OFFSET_T uiOffset;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
	pfn_t sPFN;
#else
	unsigned long uiPFN;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */
	IMG_UINT32 ui32CPUCacheFlags;
	pgprot_t sPageProt;
	IMG_CPU_PHYADDR asCpuPAddr[PMR_MAX_TRANSLATION_STACK_ALLOC];
	IMG_BOOL abValid[PMR_MAX_TRANSLATION_STACK_ALLOC];
	IMG_UINT32 uiOffsetIdx, uiNumOfPFNs;
	IMG_CPU_PHYADDR *psCpuPAddr;
	IMG_BOOL *pbValid;
#if defined(PMR_OS_USE_VM_INSERT_PAGE)
	IMG_BOOL bMixedMap = IMG_FALSE;
#endif

	eError = PMRLockSysPhysAddresses(psPMR);
	if (eError != PVRSRV_OK)
	{
		goto e0;
	}

	if (((ps_vma->vm_flags & VM_WRITE) != 0) &&
	    ((ps_vma->vm_flags & VM_SHARED) == 0))
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto e0;
	}

	sPageProt = vm_get_page_prot(ps_vma->vm_flags);

	ui32CPUCacheFlags = DevmemCPUCacheMode(psDevNode, PMR_Flags(psPMR));
	switch (ui32CPUCacheFlags)
	{
		case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
				sPageProt = pgprot_noncached(sPageProt);
				break;

		case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
				sPageProt = pgprot_writecombine(sPageProt);
				break;

		case PVRSRV_MEMALLOCFLAG_CPU_CACHED:
				break;

		default:
				eError = PVRSRV_ERROR_INVALID_PARAMS;
				goto e0;
	}
	ps_vma->vm_page_prot = sPageProt;

	uiLength = ps_vma->vm_end - ps_vma->vm_start;

	ps_vma->vm_flags |= VM_IO;

/* Don't include the mapping in core dumps */
	ps_vma->vm_flags |= VM_DONTDUMP;

	/*
	 * Disable mremap because our nopage handler assumes all
	 * page requests have already been validated.
	 */
	ps_vma->vm_flags |= VM_DONTEXPAND;

	/* Don't allow mapping to be inherited across a process fork */
	ps_vma->vm_flags |= VM_DONTCOPY;

	/* Can we use stack allocations */
	uiNumOfPFNs = uiLength >> PAGE_SHIFT;
	if (uiNumOfPFNs > PMR_MAX_TRANSLATION_STACK_ALLOC)
	{
		psCpuPAddr = OSAllocMem(uiNumOfPFNs * sizeof(*psCpuPAddr));
		if (psCpuPAddr == NULL)
		{
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto e1;
		}

		/* Should allocation fail, clean-up here before exiting */
		pbValid = OSAllocMem(uiNumOfPFNs * sizeof(*pbValid));
		if (pbValid == NULL)
		{
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			OSFreeMem(psCpuPAddr);
			goto e1;
		}
	}
	else
	{
		psCpuPAddr = asCpuPAddr;
		pbValid = abValid;
	}

	/* Obtain map range pfns */
	eError = PMR_CpuPhysAddr(psPMR,
				 PAGE_SHIFT,
				 uiNumOfPFNs,
				 0,
				 psCpuPAddr,
				 pbValid);
	if (eError != PVRSRV_OK)
	{
		goto e3;
	}

#if defined(PMR_OS_USE_VM_INSERT_PAGE)
	/*
	 * Scan the map range for pfns without struct page* handling. If
	 * we find one, this is a mixed map, and we can't use
	 * vm_insert_page()
	 */
	for (uiOffsetIdx = 0; uiOffsetIdx < uiNumOfPFNs; ++uiOffsetIdx)
	{
		if (pbValid[uiOffsetIdx])
		{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
			sPFN = phys_to_pfn_t(psCpuPAddr[uiOffsetIdx].uiAddr, 0);

			if (!pfn_t_valid(sPFN) || page_count(pfn_t_to_page(sPFN)) == 0)
#else
			uiPFN = psCpuPAddr[uiOffsetIdx].uiAddr >> PAGE_SHIFT;
			PVR_ASSERT(((IMG_UINT64)uiPFN << PAGE_SHIFT) == psCpuPAddr[uiOffsetIdx].uiAddr);

			if (!pfn_valid(uiPFN) || page_count(pfn_to_page(uiPFN)) == 0)
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */
			{
				bMixedMap = IMG_TRUE;
				break;
			}
		}
	}

	if (bMixedMap)
	{
		ps_vma->vm_flags |= VM_MIXEDMAP;
	}
#else
	ps_vma->vm_flags |= VM_PFNMAP;
#endif /* PMR_OS_USE_VM_INSERT_PAGE */

	for (uiOffset = 0; uiOffset < uiLength; uiOffset += 1ULL<<PAGE_SHIFT)
	{
		size_t uiNumContiguousBytes;
		IMG_INT32 iStatus;

		uiNumContiguousBytes = 1ULL<<PAGE_SHIFT;
		uiOffsetIdx = uiOffset >> PAGE_SHIFT;

		/*
		 * Only map in pages that are valid, any that aren't will be
		 * picked up by the nopage handler which will return a zeroed
		 * page for us.
		 */
		if (pbValid[uiOffsetIdx])
		{
#if defined(CONFIG_L4_LINUX)
			IMG_CPU_VIRTADDR pvCpuVAddr;

			/* Use L4LINUX specific function, removes per-arch codepath */
			pvCpuVAddr = l4x_phys_to_virt(psCpuPAddr[uiOffsetIdx].uiAddr);
			if (pvCpuVAddr == NULL)
			{
				eError = PVRSRV_ERROR_DEVICEMEM_MAP_FAILED;
				goto e3;
			}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
			sPFN = phys_to_pfn_t(pvCpuVAddr, 0);
#else
			uiPFN = ((uintptr_t) pvCpuVAddr) >> PAGE_SHIFT;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */
#else /* defined(CONFIG_L4_LINUX) */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
			sPFN = phys_to_pfn_t(psCpuPAddr[uiOffsetIdx].uiAddr, 0);
#else
			uiPFN = psCpuPAddr[uiOffsetIdx].uiAddr >> PAGE_SHIFT;
			PVR_ASSERT(((IMG_UINT64)uiPFN << PAGE_SHIFT) == psCpuPAddr[uiOffsetIdx].uiAddr);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */
#endif /* defined(CONFIG_L4_LINUX) */

#if defined(PMR_OS_USE_VM_INSERT_PAGE)
			if (bMixedMap)
			{
				/*
				 * This path is just for debugging. It should be
				 * equivalent to the remap_pfn_range() path.
				 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
				iStatus = vm_insert_mixed(ps_vma,
				                          ps_vma->vm_start + uiOffset,
				                          sPFN);
#else
				iStatus = vm_insert_mixed(ps_vma,
				                          ps_vma->vm_start + uiOffset,
				                          uiPFN);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */
			}
			else
			{
				/* Since kernel 3.7 this sets VM_MIXEDMAP internally */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
				iStatus = vm_insert_page(ps_vma,
				                         ps_vma->vm_start + uiOffset,
				                         pfn_t_to_page(sPFN));
#else
				iStatus = vm_insert_page(ps_vma,
				                         ps_vma->vm_start + uiOffset,
				                         pfn_to_page(uiPFN));
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */
			}
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
			iStatus = remap_pfn_range(ps_vma,
			                          ps_vma->vm_start + uiOffset,
			                          pfn_t_to_pfn(sPFN),
			                          uiNumContiguousBytes,
			                          ps_vma->vm_page_prot);
#else
			iStatus = remap_pfn_range(ps_vma,
			                          ps_vma->vm_start + uiOffset,
			                          uiPFN,
			                          uiNumContiguousBytes,
			                          ps_vma->vm_page_prot);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)) */
#endif  /* PMR_OS_USE_VM_INSERT_PAGE */

			PVR_ASSERT(iStatus == 0);
			if(iStatus)
			{
				/*
				 * N.B. not the right error code but it doesn't
				 * get propagated anyway :(
				 */
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e3;
			}
		}
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && defined(PVRSRV_ENABLE_MEMORY_STATS)
		PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_MAP_UMA_LMA_PAGES,
					     (void*)(uintptr_t)(ps_vma->vm_start + uiOffset),
					     psCpuPAddr[uiOffsetIdx],
					     PAGE_SIZE,
					     NULL);
#endif
	}
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_ENABLE_MEMORY_STATS)
		PVRSRVStatsIncrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_MAP_UMA_LMA_PAGES, uiNumOfPFNs * PAGE_SIZE);
#endif
	if (psCpuPAddr != asCpuPAddr)
	{
		OSFreeMem(psCpuPAddr);
		OSFreeMem(pbValid);
	}

	/* let us see the PMR so we can unlock it later */
	ps_vma->vm_private_data = psPMR;

	/* Install open and close handlers for ref-counting */
	ps_vma->vm_ops = &gsMMapOps;

	/*
	 * Take a reference on the PMR so that it can't be freed while mapped
	 * into the user process.
	 */
	PMRRefPMR(psPMR);

	return PVRSRV_OK;

	/* Error exit paths follow */
 e3:
	if (psCpuPAddr != asCpuPAddr)
	{
		OSFreeMem(psCpuPAddr);
		OSFreeMem(pbValid);
	}
 e1:
	PVR_DPF((PVR_DBG_ERROR, "don't know how to handle this error.  Abort!"));
	PMRUnlockSysPhysAddresses(psPMR);
 e0:
	return eError;
}
