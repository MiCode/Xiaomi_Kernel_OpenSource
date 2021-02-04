/*************************************************************************/ /*!
@File
@Title          Implementation of PMR functions for OS managed memory
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Part of the memory management.  This module is responsible for
                implementing the function callbacks for physical memory borrowed
                from that normally managed by the operating system.
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
#include <linux/version.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/mm_types.h>
#include <linux/vmalloc.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <asm/io.h>
#include <asm/dma-mapping.h>
#if defined(CONFIG_X86)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0))
#include <asm/set_memory.h>
#else
#include <asm/cacheflush.h>
#endif
#endif

/* include/ */
#include "rgx_heaps.h"
#include "img_types.h"
#include "pvr_debug.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"
#include "rgx_pdump_panics.h"
/* services/server/include/ */
#include "allocmem.h"
#include "osfunc.h"
#include "pdump_km.h"
#include "pmr.h"
#include "pmr_impl.h"
#include "cache_km.h"
#include "devicemem_server_utils.h"

/* ourselves */
#include "physmem_osmem.h"
#include "physmem_osmem_linux.h"

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif

#include "kernel_compatibility.h"
#include "ged_log.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
static IMG_UINT32 g_uiMaxOrder = PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM;
#else
/* split_page not available on older kernels */
#undef PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM
#define PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM 0
static IMG_UINT32 g_uiMaxOrder;
#endif

/*
   These corresponds to the MMU min/max page sizes and associated PTE
   alignment that can be used on the device for an allocation. It is
   4KB (min) and 2MB (max) respectively.
*/
#define PVR_MIN_PHYSMEM_CONTIG_ALLOC_LOG2PGSZ	RGX_HEAP_4KB_PAGE_SHIFT
#define PVR_MAX_PHYSMEM_CONTIG_ALLOC_LOG2PGSZ	RGX_HEAP_2MB_PAGE_SHIFT

/* Defines how many pages should be mapped at once to the kernel */
#define PVR_LINUX_PHYSMEM_MAX_KMAP_PAGES 1024 /* 4 MB */

/*
	These are used to get/set/mask lower-order bits in a dma_addr_t
	to provide side-band information associated with that address.
	These includes whether the address was obtained via alloc_page
	or dma_alloc and if address came allocated pre-aligned or an
	adjustment was made manually to aligned it.
*/
#define DMA_SET_ADJUSTED_ADDR(x)		((x) | ((dma_addr_t)0x02))
#define DMA_IS_ADDR_ADJUSTED(x)			((x) & ((dma_addr_t)0x02))
#define DMA_SET_ALLOCPG_ADDR(x)			((x) | ((dma_addr_t)0x01))
#define DMA_IS_ALLOCPG_ADDR(x)			((x) & ((dma_addr_t)0x01))
#define DMA_GET_ALIGN_ADJUSTMENT(x)		((x>>2) & ((dma_addr_t)0x3ff))
#define DMA_SET_ALIGN_ADJUSTMENT(x,y)	((x) | (((dma_addr_t)y)<<0x02))
#define DMA_GET_ADDR(x)					(((dma_addr_t)x) & ((dma_addr_t)~0xfff))
#define DMA_VADDR_NOT_IN_USE			0xCAFEF00DDEADBEEFULL

typedef struct _PMR_OSPAGEARRAY_DATA_ {
	/* Device for which this allocation has been made */
	PVRSRV_DEVICE_NODE *psDevNode;
	/* The pid that made this allocation */
	IMG_PID uiPid;

	/*
	 * iNumOSPagesAllocated:
	 * Number of pages allocated in this PMR so far.
	 * This allows for up to (2^31 - 1) pages. With 4KB pages, that's 8TB of memory for each PMR.
	 */
	IMG_INT32 iNumOSPagesAllocated;

	/*
	 * uiTotalNumOSPages:
	 * Total number of pages supported by this PMR. (Fixed as of now due the fixed Page table array size)
	 *  number of "pages" (a.k.a. macro pages, compound pages, higher order pages, etc...)
	 */
	IMG_UINT32 uiTotalNumOSPages;

	/*
	  uiLog2AllocPageSize;

	  size of each "page" -- this would normally be the same as
	  PAGE_SHIFT, but we support the idea that we may allocate pages
	  in larger chunks for better contiguity, using order>0 in the
	  call to alloc_pages()
	*/
	IMG_UINT32 uiLog2AllocPageSize;

	/*
	  ui64DmaMask;
	*/
	IMG_UINT64 ui64DmaMask;

	/*
	  For non DMA/CMA allocation, pagearray references the pages
	  thus allocated; one entry per compound page when compound
	  pages are used. In addition, for DMA/CMA allocations, we
	  track the returned cpu virtual and device bus address.
	*/
	struct page **pagearray;
	dma_addr_t *dmaphysarray;
	void **dmavirtarray;

	/*
	  Record at alloc time whether poisoning will be required when the
	  PMR is freed.
	*/
	IMG_BOOL bZero;
	IMG_BOOL bPoisonOnFree;
	IMG_BOOL bPoisonOnAlloc;
	IMG_BOOL bOnDemand;
	IMG_BOOL bUnpinned; /* Should be protected by page pool lock */
	IMG_BOOL bIsCMA; /* Is CMA memory allocated via DMA framework */

	/*
	  The cache mode of the PMR. Additionally carrying the CPU-Cache-Clean
	  flag, advising us to do cache maintenance on behalf of the caller.
	  Boolean used to track if we need to revert the cache attributes
	  of the pages used in this allocation. Depends on OS/architecture.
	*/
	IMG_UINT32 ui32CPUCacheFlags;
	IMG_BOOL bUnsetMemoryType;
} PMR_OSPAGEARRAY_DATA;

/***********************************
 * Page pooling for uncached pages *
 ***********************************/

static INLINE void
_FreeOSPage_CMA(struct device *dev,
				size_t alloc_size,
				IMG_UINT32 uiOrder,
				void *virt_addr,
				dma_addr_t dev_addr,
				struct page *psPage);

static void
_FreeOSPage(IMG_UINT32 uiOrder,
			IMG_BOOL bUnsetMemoryType,
			struct page *psPage);

static PVRSRV_ERROR
_FreeOSPages(PMR_OSPAGEARRAY_DATA *psPageArrayData,
			IMG_UINT32 *pai32FreeIndices,
			IMG_UINT32 ui32FreePageCount);

static PVRSRV_ERROR
_FreePagesFromPoolUnlocked(IMG_UINT32 uiMaxPagesToFree,
						   IMG_UINT32 *puiPagesFreed);

/* A struct for our page pool holding an array of zeroed (!) pages.
 * We always put units of page arrays to the pool but are
 * able to take individual pages */
typedef	struct
{
	/* Linkage for page pool LRU list */
	struct list_head sPagePoolItem;

	/* How many items are still in the page array */
	IMG_UINT32 uiItemsRemaining;
	/* Array of the actual pages */
	struct page **ppsPageArray;

} LinuxPagePoolEntry;

/* CleanupThread structure to put allocation in page pool */
typedef struct
{
	PVRSRV_CLEANUP_THREAD_WORK sCleanupWork;
	IMG_UINT32 ui32CPUCacheMode;
	LinuxPagePoolEntry *psPoolEntry;
} LinuxCleanupData;

/* A struct for the unpinned items */
typedef struct
{
	struct list_head sUnpinPoolItem;
	PMR_OSPAGEARRAY_DATA *psPageArrayDataPtr;
} LinuxUnpinEntry;


/* Caches to hold page pool and page array structures */
static struct kmem_cache *g_psLinuxPagePoolCache;
static struct kmem_cache *g_psLinuxPageArray;

/* Track what is live, all protected by pool lock.
 * x86 needs two page pools because we have to change the memory attributes
 * of the pages which is expensive due to an implicit flush.
 * See set_pages_array_uc/wc/wb. */
static IMG_UINT32 g_ui32UnpinPageCount;
static IMG_UINT32 g_ui32PagePoolUCCount;
#if defined(CONFIG_X86)
static IMG_UINT32 g_ui32PagePoolWCCount;
#endif
/* Tracks asynchronous tasks currently accessing the page pool.
 * It is incremented if a defer free task
 * is created. Both will decrement the value when they finished the work.
 * The atomic prevents piling up of deferred work in case the deferred thread
 * cannot keep up with the application.*/
static ATOMIC_T g_iPoolCleanTasks;
/* We don't want too many asynchronous threads trying to access the page pool
 * at the same time */
#define PVR_LINUX_PHYSMEM_MAX_ASYNC_CLEAN_TASKS 128

/* Defines how many pages the page cache should hold. */
#if defined(PVR_LINUX_PHYSMEM_MAX_POOL_PAGES)
static const IMG_UINT32 g_ui32PagePoolMaxEntries = PVR_LINUX_PHYSMEM_MAX_POOL_PAGES;
#else
static const IMG_UINT32 g_ui32PagePoolMaxEntries;
#endif

/* 	We double check if we would exceed this limit if we are below MAX_POOL_PAGES
	and want to add an allocation to the pool.
	This prevents big allocations being given back to the OS just because they
	exceed the MAX_POOL_PAGES limit even though the pool is currently empty. */
#if defined(PVR_LINUX_PHYSMEM_MAX_EXCESS_POOL_PAGES)
static const IMG_UINT32 g_ui32PagePoolMaxExcessEntries = PVR_LINUX_PHYSMEM_MAX_EXCESS_POOL_PAGES;
#else
static const IMG_UINT32 g_ui32PagePoolMaxExcessEntries;
#endif

#if defined(CONFIG_X86)
#define PHYSMEM_OSMEM_NUM_OF_POOLS 2
static const IMG_UINT32 g_aui32CPUCacheFlags[PHYSMEM_OSMEM_NUM_OF_POOLS] = {
	PVRSRV_MEMALLOCFLAG_CPU_UNCACHED,
	PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE
};
#else
#define PHYSMEM_OSMEM_NUM_OF_POOLS 1
static const IMG_UINT32 g_aui32CPUCacheFlags[PHYSMEM_OSMEM_NUM_OF_POOLS] = {
	PVRSRV_MEMALLOCFLAG_CPU_UNCACHED
};
#endif

/* Global structures we use to manage the page pool */
static DEFINE_MUTEX(g_sPagePoolMutex);

/* List holding the page array pointers: */
static LIST_HEAD(g_sPagePoolList_WC);
static LIST_HEAD(g_sPagePoolList_UC);
static LIST_HEAD(g_sUnpinList);

static inline IMG_UINT32
_PagesInPoolUnlocked(void)
{
	IMG_UINT32 uiCnt = g_ui32PagePoolUCCount;
#if defined(CONFIG_X86)
	uiCnt += g_ui32PagePoolWCCount;
#endif
	return uiCnt;
}

static inline void
_PagePoolLock(void)
{
	mutex_lock(&g_sPagePoolMutex);
}

static inline int
_PagePoolTrylock(void)
{
	return mutex_trylock(&g_sPagePoolMutex);
}

static inline void
_PagePoolUnlock(void)
{
	mutex_unlock(&g_sPagePoolMutex);
}

static PVRSRV_ERROR
_AddUnpinListEntryUnlocked(PMR_OSPAGEARRAY_DATA *psOSPageArrayData)
{
	LinuxUnpinEntry *psUnpinEntry;

	psUnpinEntry = OSAllocMem(sizeof(*psUnpinEntry));
	if (!psUnpinEntry)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: OSAllocMem failed. Cannot add entry to unpin list.",
				__func__));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psUnpinEntry->psPageArrayDataPtr = psOSPageArrayData;

	/* Add into pool that the shrinker can access easily*/
	list_add_tail(&psUnpinEntry->sUnpinPoolItem, &g_sUnpinList);

	g_ui32UnpinPageCount += psOSPageArrayData->iNumOSPagesAllocated;

	return PVRSRV_OK;
}

static void
_RemoveUnpinListEntryUnlocked(PMR_OSPAGEARRAY_DATA *psOSPageArrayData)
{
	LinuxUnpinEntry *psUnpinEntry, *psTempUnpinEntry;

	/* Remove from pool */
	list_for_each_entry_safe(psUnpinEntry,
	                         psTempUnpinEntry,
	                         &g_sUnpinList,
	                         sUnpinPoolItem)
	{
		if (psUnpinEntry->psPageArrayDataPtr == psOSPageArrayData)
		{
			list_del(&psUnpinEntry->sUnpinPoolItem);
			break;
		}
	}

	OSFreeMem(psUnpinEntry);

	g_ui32UnpinPageCount -= psOSPageArrayData->iNumOSPagesAllocated;
}

static inline IMG_BOOL
_GetPoolListHead(IMG_UINT32 ui32CPUCacheFlags,
				 struct list_head **ppsPoolHead,
				 IMG_UINT32 **ppuiCounter)
{
	switch(PVRSRV_CPU_CACHE_MODE(ui32CPUCacheFlags))
	{
		case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
#if defined(CONFIG_X86)
		/*
			For x86 we need to keep different lists for uncached
			and write-combined as we must always honour the PAT
			setting which cares about this difference.
		*/

			*ppsPoolHead = &g_sPagePoolList_WC;
			*ppuiCounter = &g_ui32PagePoolWCCount;
			break;
#endif

		case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
			*ppsPoolHead = &g_sPagePoolList_UC;
			*ppuiCounter = &g_ui32PagePoolUCCount;
			break;

		default:
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Unknown CPU caching mode. "
					 "Using default UC pool.",
					 __func__));
			*ppsPoolHead = &g_sPagePoolList_UC;
			*ppuiCounter = &g_ui32PagePoolUCCount;
			PVR_ASSERT(0);
			return IMG_FALSE;
	}
	return IMG_TRUE;
}

static struct shrinker g_sShrinker;

/* Returning the number of pages that still reside in the page pool. */
static unsigned long
_GetNumberOfPagesInPoolUnlocked(void)
{
	return _PagesInPoolUnlocked() + g_ui32UnpinPageCount;
}

/* Linux shrinker function that informs the OS about how many pages we are caching and
 * it is able to reclaim. */
static unsigned long
_CountObjectsInPagePool(struct shrinker *psShrinker, struct shrink_control *psShrinkControl)
{
	int remain;

	PVR_ASSERT(psShrinker == &g_sShrinker);
	(void)psShrinker;
	(void)psShrinkControl;

	/* In order to avoid possible deadlock use mutex_trylock in place of mutex_lock */
	if (_PagePoolTrylock() == 0)
		return 0;
	remain = _GetNumberOfPagesInPoolUnlocked();
	_PagePoolUnlock();

	return remain;
}

/* Linux shrinker function to reclaim the pages from our page pool */
static unsigned long
_ScanObjectsInPagePool(struct shrinker *psShrinker, struct shrink_control *psShrinkControl)
{
	unsigned long uNumToScan = psShrinkControl->nr_to_scan;
	unsigned long uSurplus = 0;
	LinuxUnpinEntry *psUnpinEntry, *psTempUnpinEntry;
	IMG_UINT32 uiPagesFreed;

	PVR_ASSERT(psShrinker == &g_sShrinker);
	(void)psShrinker;

	/* In order to avoid possible deadlock use mutex_trylock in place of mutex_lock */
	if (_PagePoolTrylock() == 0)
		return SHRINK_STOP;

	_FreePagesFromPoolUnlocked(uNumToScan,
							   &uiPagesFreed);
	uNumToScan -= uiPagesFreed;

	if (uNumToScan == 0)
	{
		goto e_exit;
	}

	/* Free unpinned memory, starting with LRU entries */
	list_for_each_entry_safe(psUnpinEntry,
							 psTempUnpinEntry,
							 &g_sUnpinList,
							 sUnpinPoolItem)
	{
		PMR_OSPAGEARRAY_DATA *psPageArrayDataPtr = psUnpinEntry->psPageArrayDataPtr;
		IMG_UINT32 uiNumPages = (psPageArrayDataPtr->uiTotalNumOSPages > psPageArrayDataPtr->iNumOSPagesAllocated)?
								psPageArrayDataPtr->iNumOSPagesAllocated:psPageArrayDataPtr->uiTotalNumOSPages;
		PVRSRV_ERROR eError;

		/* Free associated pages */
		eError = _FreeOSPages(psPageArrayDataPtr,
							  NULL,
							  0);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Shrinker is unable to free unpinned pages. Error: %s (%d)",
					 __FUNCTION__,
					 PVRSRVGetErrorStringKM(eError),
					 eError));
			goto e_exit;
		}

		/* Remove item from pool */
		list_del(&psUnpinEntry->sUnpinPoolItem);

		g_ui32UnpinPageCount -= uiNumPages;

		/* Check if there is more to free or if we already surpassed the limit */
		if (uiNumPages < uNumToScan)
		{
			uNumToScan -= uiNumPages;

		}
		else if (uiNumPages > uNumToScan)
		{
			uSurplus += uiNumPages - uNumToScan;
			uNumToScan = 0;
			goto e_exit;
		}
		else
		{
			uNumToScan -= uiNumPages;
			goto e_exit;
		}
	}

e_exit:
	if (list_empty(&g_sUnpinList))
	{
		PVR_ASSERT(g_ui32UnpinPageCount == 0);
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0))
	{
		int remain;
		remain = _GetNumberOfPagesInPoolUnlocked();
		_PagePoolUnlock();
		return remain;
	}
#else
	/* Returning the number of pages freed during the scan */
	_PagePoolUnlock();
	return psShrinkControl->nr_to_scan - uNumToScan + uSurplus;
#endif
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0))
static int
_ShrinkPagePool(struct shrinker *psShrinker, struct shrink_control *psShrinkControl)
{
	if (psShrinkControl->nr_to_scan != 0)
	{
		return _ScanObjectsInPagePool(psShrinker, psShrinkControl);
	}
	else
	{
		/* No pages are being reclaimed so just return the page count */
		return _CountObjectsInPagePool(psShrinker, psShrinkControl);
	}
}

static struct shrinker g_sShrinker =
{
	.shrink = _ShrinkPagePool,
	.seeks = DEFAULT_SEEKS
};
#else
static struct shrinker g_sShrinker =
{
	.count_objects = _CountObjectsInPagePool,
	.scan_objects = _ScanObjectsInPagePool,
	.seeks = DEFAULT_SEEKS
};
#endif

/* Register the shrinker so Linux can reclaim cached pages */
void LinuxInitPhysmem(void)
{
	g_psLinuxPageArray = kmem_cache_create("pvr-pa", sizeof(PMR_OSPAGEARRAY_DATA), 0, 0, NULL);

	_PagePoolLock();
	g_psLinuxPagePoolCache = kmem_cache_create("pvr-pp", sizeof(LinuxPagePoolEntry), 0, 0, NULL);
	if (g_psLinuxPagePoolCache)
	{
		/* Only create the shrinker if we created the cache OK */
		register_shrinker(&g_sShrinker);
	}
	_PagePoolUnlock();

	OSAtomicWrite(&g_iPoolCleanTasks, 0);
}

/* Unregister the shrinker and remove all pages from the pool that are still left */
void LinuxDeinitPhysmem(void)
{
	IMG_UINT32 uiPagesFreed;

	if (OSAtomicRead(&g_iPoolCleanTasks) > 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "Still deferred cleanup tasks running "
				"while deinitialising memory subsystem."));
	}

	_PagePoolLock();
	if (_FreePagesFromPoolUnlocked(IMG_UINT32_MAX, &uiPagesFreed) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "Unable to free all pages from page pool when "
				"deinitialising memory subsystem."));
		PVR_ASSERT(0);
	}

	PVR_ASSERT(_PagesInPoolUnlocked() == 0);

	/* Free the page cache */
	kmem_cache_destroy(g_psLinuxPagePoolCache);

	unregister_shrinker(&g_sShrinker);
	_PagePoolUnlock();

	kmem_cache_destroy(g_psLinuxPageArray);
}

static void EnableOOMKiller(void)
{
	current->flags &= ~PF_DUMPCORE;
}

static void DisableOOMKiller(void)
{
	/* PF_DUMPCORE is treated by the VM as if the OOM killer was disabled.
	 *
	 * As oom_killer_disable() is an inline, non-exported function, we
	 * can't use it from a modular driver. Furthermore, the OOM killer
	 * API doesn't look thread safe, which `current' is.
	 */
	WARN_ON(current->flags & PF_DUMPCORE);
	current->flags |= PF_DUMPCORE;
}

/* Prints out the addresses in a page array for debugging purposes
 * Define PHYSMEM_OSMEM_DEBUG_DUMP_PAGE_ARRAY locally to activate: */
/* #define PHYSMEM_OSMEM_DEBUG_DUMP_PAGE_ARRAY 1 */
static inline void
_DumpPageArray(struct page **pagearray, IMG_UINT32 uiPagesToPrint)
{
#if defined(PHYSMEM_OSMEM_DEBUG_DUMP_PAGE_ARRAY)
	IMG_UINT32 i;
	if (pagearray)
	{
		printk("Array %p:\n", pagearray);
		for (i = 0; i < uiPagesToPrint; i++)
		{
			printk("%p | ", (pagearray)[i]);
		}
		printk("\n");
	}
	else
	{
		printk("Array is NULL:\n");
	}
#else
	PVR_UNREFERENCED_PARAMETER(pagearray);
	PVR_UNREFERENCED_PARAMETER(uiPagesToPrint);
#endif
}

/* Debugging function that dumps out the number of pages for every
 * page array that is currently in the page pool.
 * Not defined by default. Define locally to activate feature: */
/* #define PHYSMEM_OSMEM_DEBUG_DUMP_PAGE_POOL 1 */
static void
_DumpPoolStructure(void)
{
#if defined(PHYSMEM_OSMEM_DEBUG_DUMP_PAGE_POOL)
	LinuxPagePoolEntry *psPagePoolEntry, *psTempPoolEntry;
	struct list_head *psPoolHead = NULL;
	IMG_UINT32  j;
	IMG_UINT32 *puiCounter;

	printk("\n");
	/* Empty all pools */
	for (j = 0; j < PHYSMEM_OSMEM_NUM_OF_POOLS; j++)
	{

		printk("pool = %u \n", j);

		/* Get the correct list for this caching mode */
		if (!_GetPoolListHead(g_aui32CPUCacheFlags[j], &psPoolHead, &puiCounter))
		{
			break;
		}

		list_for_each_entry_safe(psPagePoolEntry,
								 psTempPoolEntry,
								 psPoolHead,
								 sPagePoolItem)
		{
			printk("%u | ", psPagePoolEntry->uiItemsRemaining);
		}
		printk("\n");
	}
#endif
}

/* Free a certain number of pages from the page pool.
 * Mainly used in error paths or at deinitialisation to
 * empty the whole pool. */
static PVRSRV_ERROR
_FreePagesFromPoolUnlocked(IMG_UINT32 uiMaxPagesToFree,
						   IMG_UINT32 *puiPagesFreed)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	LinuxPagePoolEntry *psPagePoolEntry, *psTempPoolEntry;
	struct list_head *psPoolHead = NULL;
	IMG_UINT32 i, j;
	IMG_UINT32 *puiCounter;

	*puiPagesFreed = uiMaxPagesToFree;

	/* Empty all pools */
	for (j = 0; j < PHYSMEM_OSMEM_NUM_OF_POOLS; j++)
	{

		/* Get the correct list for this caching mode */
		if (!_GetPoolListHead(g_aui32CPUCacheFlags[j], &psPoolHead, &puiCounter))
		{
			break;
		}

		/* Free the pages and remove page arrays from the pool if they are exhausted */
		list_for_each_entry_safe(psPagePoolEntry,
								 psTempPoolEntry,
								 psPoolHead,
								 sPagePoolItem)
		{
			IMG_UINT32 uiItemsToFree;
			struct page **ppsPageArray;

			/* Check if we are going to free the whole page array or just parts */
			if (psPagePoolEntry->uiItemsRemaining <= uiMaxPagesToFree)
			{
				uiItemsToFree = psPagePoolEntry->uiItemsRemaining;
				ppsPageArray = psPagePoolEntry->ppsPageArray;
			}
			else
			{
				uiItemsToFree = uiMaxPagesToFree;
				ppsPageArray = &(psPagePoolEntry->ppsPageArray[psPagePoolEntry->uiItemsRemaining - uiItemsToFree]);
			}

#if defined(CONFIG_X86)
			/* Set the correct page caching attributes on x86 */
			if (!PVRSRV_CHECK_CPU_CACHED(g_aui32CPUCacheFlags[j]))
			{
				int ret;
				ret = set_pages_array_wb(ppsPageArray, uiItemsToFree);
				if (ret)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s: Failed to reset page attributes", __FUNCTION__));
					eError = PVRSRV_ERROR_FAILED_TO_FREE_PAGES;
					goto e_exit;
				}
			}
#endif

			/* Free the actual pages */
			for (i = 0; i < uiItemsToFree; i++)
			{
				__free_pages(ppsPageArray[i], 0);
				ppsPageArray[i] = NULL;
			}

			/* Reduce counters */
			uiMaxPagesToFree -= uiItemsToFree;
			*puiCounter -= uiItemsToFree;
			psPagePoolEntry->uiItemsRemaining -= uiItemsToFree;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
			/*
			 * MemStats usually relies on having the bridge lock held, however
			 * the page pool code may call PVRSRVStatsIncrMemAllocPoolStat and
			 * PVRSRVStatsDecrMemAllocPoolStat without the bridge lock held, so
			 * the page pool lock is used to ensure these calls are mutually
			 * exclusive
			 */
			PVRSRVStatsDecrMemAllocPoolStat(PAGE_SIZE * uiItemsToFree);
#endif

			/* Is this pool entry exhausted, delete it */
			if (psPagePoolEntry->uiItemsRemaining == 0)
			{
				OSFreeMemNoStats(psPagePoolEntry->ppsPageArray);
				list_del(&psPagePoolEntry->sPagePoolItem);
				kmem_cache_free(g_psLinuxPagePoolCache, psPagePoolEntry);
			}

			/* Return if we have all our pages */
			if (uiMaxPagesToFree == 0)
			{
				goto e_exit;
			}
		}
	}

e_exit:
	*puiPagesFreed -= uiMaxPagesToFree;
	_DumpPoolStructure();
	return eError;
}

/* Get a certain number of pages from the page pool and
 * copy them directly into a given page array. */
static void
_GetPagesFromPoolUnlocked(IMG_UINT32 ui32CPUCacheFlags,
						  IMG_UINT32 uiMaxNumPages,
						  struct page **ppsPageArray,
						  IMG_UINT32 *puiNumReceivedPages)
{
	LinuxPagePoolEntry *psPagePoolEntry, *psTempPoolEntry;
	struct list_head *psPoolHead = NULL;
	IMG_UINT32 i;
	IMG_UINT32 *puiCounter;

	*puiNumReceivedPages = 0;

	/* Get the correct list for this caching mode */
	if (!_GetPoolListHead(ui32CPUCacheFlags, &psPoolHead, &puiCounter))
	{
		return;
	}

	/* Check if there are actually items in the list */
	if (list_empty(psPoolHead))
	{
		return;
	}

	PVR_ASSERT(*puiCounter > 0);

	/* Receive pages from the pool */
	list_for_each_entry_safe(psPagePoolEntry,
							 psTempPoolEntry,
							 psPoolHead,
							 sPagePoolItem)
	{
		/* Get the pages from this pool entry */
		for (i = psPagePoolEntry->uiItemsRemaining; i != 0 && *puiNumReceivedPages < uiMaxNumPages; i--)
		{
			ppsPageArray[*puiNumReceivedPages] = psPagePoolEntry->ppsPageArray[i-1];
			(*puiNumReceivedPages)++;
			psPagePoolEntry->uiItemsRemaining--;
		}

		/* Is this pool entry exhausted, delete it */
		if (psPagePoolEntry->uiItemsRemaining == 0)
		{
			OSFreeMemNoStats(psPagePoolEntry->ppsPageArray);
			list_del(&psPagePoolEntry->sPagePoolItem);
			kmem_cache_free(g_psLinuxPagePoolCache, psPagePoolEntry);
		}

		/* Return if we have all our pages */
		if (*puiNumReceivedPages == uiMaxNumPages)
		{
			goto exit_ok;
		}
	}

exit_ok:

	/* Update counters */
	*puiCounter -= *puiNumReceivedPages;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	/* MemStats usually relies on having the bridge lock held, however
	 * the page pool code may call PVRSRVStatsIncrMemAllocPoolStat and
	 * PVRSRVStatsDecrMemAllocPoolStat without the bridge lock held, so
	 * the page pool lock is used to ensure these calls are mutually
	 * exclusive
	 */
	PVRSRVStatsDecrMemAllocPoolStat(PAGE_SIZE * (*puiNumReceivedPages));
#endif

	_DumpPoolStructure();
	return;
}

/* Same as _GetPagesFromPoolUnlocked but handles locking and
 * checks first whether pages from the pool are a valid option. */
static inline void
_GetPagesFromPoolLocked(PVRSRV_DEVICE_NODE *psDevNode,
						IMG_UINT32 ui32CPUCacheFlags,
						IMG_UINT32 uiPagesToAlloc,
						IMG_UINT32 uiOrder,
						IMG_BOOL bZero,
						struct page **ppsPageArray,
						IMG_UINT32 *puiPagesFromPool)
{
#if defined(PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES)
	PVR_UNREFERENCED_PARAMETER(bZero);
#else
	/* Don't get pages from pool if it doesn't provide zeroed pages */
	if (bZero)
	{
		return;
	}
#endif

	/* The page pool stores only order 0 pages. If we need zeroed memory we
	 * directly allocate from the OS because it is faster than
	 * doing it within the driver. */
	if (uiOrder == 0 &&
	    !PVRSRV_CHECK_CPU_CACHED(ui32CPUCacheFlags))
	{

		_PagePoolLock();
		_GetPagesFromPoolUnlocked(ui32CPUCacheFlags,
								  uiPagesToAlloc,
								  ppsPageArray,
								  puiPagesFromPool);
		_PagePoolUnlock();
	}

	return;
}

/* Takes a page array and maps it into the kernel to write zeros */
static PVRSRV_ERROR
_ZeroPageArray(IMG_UINT32 uiNumToClean,
               struct page **ppsCleanArray,
               pgprot_t pgprot)
{
	IMG_CPU_VIRTADDR pvAddr;
	IMG_UINT32 uiMaxPagesToMap = MIN(PVR_LINUX_PHYSMEM_MAX_KMAP_PAGES,
	                                 uiNumToClean);

	/* Map and fill the pages with zeros.
	 * For large page arrays do it PVR_LINUX_PHYSMEM_MAX_KMAP_SIZE
	 * at a time. */
	while (uiNumToClean != 0)
	{
		IMG_UINT32 uiToClean = (uiNumToClean >= uiMaxPagesToMap) ?
		                        uiMaxPagesToMap :
		                        uiNumToClean;

#if !defined(CONFIG_64BIT) || defined(PVRSRV_FORCE_SLOWER_VMAP_ON_64BIT_BUILDS)
		pvAddr = vmap(ppsCleanArray, uiToClean, VM_WRITE, pgprot);
#else
		pvAddr = vm_map_ram(ppsCleanArray, uiToClean, -1, pgprot);
#endif
		if (!pvAddr)
		{
			if (uiMaxPagesToMap <= 1)
			{
				PVR_DPF((PVR_DBG_ERROR,
						"%s: Out of vmalloc memory, "
						"unable to map pages for zeroing.",
						__func__));
				return PVRSRV_ERROR_OUT_OF_MEMORY;
			}
			else
			{
				/* Halve the pages to map at once and try again. */
				uiMaxPagesToMap = uiMaxPagesToMap >> 1;
				continue;
			}
		}

		OSDeviceMemSet(pvAddr, 0, PAGE_SIZE * uiToClean);

#if !defined(CONFIG_64BIT) || defined(PVRSRV_FORCE_SLOWER_VMAP_ON_64BIT_BUILDS)
		vunmap(pvAddr);
#else
		vm_unmap_ram(pvAddr, uiToClean);
#endif

		ppsCleanArray = &(ppsCleanArray[uiToClean]);
		uiNumToClean -= uiToClean;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR
_CleanupThread_CleanPages(void *pvData)
{
	LinuxCleanupData *psCleanupData = (LinuxCleanupData*) pvData;
	LinuxPagePoolEntry *psPagePoolEntry = psCleanupData->psPoolEntry;
	struct list_head *psPoolHead = NULL;
	IMG_UINT32 *puiCounter = NULL;
#if defined(PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES)
	PVRSRV_ERROR eError;
	pgprot_t pgprot;
	IMG_UINT32 i;
#endif /* defined(PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES) */

	/* Get the correct pool for this caching mode. */
	_GetPoolListHead(psCleanupData->ui32CPUCacheMode , &psPoolHead, &puiCounter);

#if defined(PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES)
	switch(PVRSRV_CPU_CACHE_MODE(psCleanupData->ui32CPUCacheMode))
	{
		case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
#if defined(CONFIG_X86)
			/* For x86 we can only map with the same attributes
			 * as in the PAT settings*/
			pgprot = pgprot_noncached(PAGE_KERNEL);
			break;
#endif

		case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
			pgprot = pgprot_writecombine(PAGE_KERNEL);
			break;

		default:
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Unknown caching mode to set page protection flags.",
					__func__));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto eExit;
	}

	/* Map and fill the pages with zeros.
	 * For large page arrays do it PVR_LINUX_PHYSMEM_MAX_KMAP_SIZE
	 * at a time. */
	eError = _ZeroPageArray(psPagePoolEntry->uiItemsRemaining,
	                        psPagePoolEntry->ppsPageArray,
	                        pgprot);
	if (eError != PVRSRV_OK)
	{
		goto eExit;
	}
#endif /* defined(PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES) */

	/* Lock down pool and add item */
	_PagePoolLock();

	/* Pool counters were already updated so don't do it here again*/

	/* The pages are all zeroed so return them to the pool. */
	list_add_tail(&psPagePoolEntry->sPagePoolItem, psPoolHead);

	_DumpPoolStructure();
	_PagePoolUnlock();

	OSFreeMem(pvData);
	OSAtomicDecrement(&g_iPoolCleanTasks);

	return PVRSRV_OK;

#if defined(PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES)
eExit:
	/* we failed to zero the pages so return the error so we can
	 * retry during the next spin */
	if ((psCleanupData->sCleanupWork.ui32RetryCount - 1) > 0)
	{
		return eError;
	}

	/* this was the last retry, give up and free pages to OS */
	PVR_DPF((PVR_DBG_ERROR,
			"%s: Deferred task error, freeing pages to OS.",
			__func__));
	_PagePoolLock();

	*puiCounter -= psPagePoolEntry->uiItemsRemaining;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	/* MemStats usually relies on having the bridge lock held, however
	 * the page pool code may call PVRSRVStatsIncrMemAllocPoolStat and
	 * PVRSRVStatsDecrMemAllocPoolStat without the bridge lock held, so
	 * the page pool lock is used to ensure these calls are mutually
	 * exclusive
	 */
	PVRSRVStatsDecrMemAllocPoolStat(PAGE_SIZE * psCleanupData->psPoolEntry->uiItemsRemaining);
#endif

	_PagePoolUnlock();

	for (i = 0; i < psCleanupData->psPoolEntry->uiItemsRemaining; i++)
	{
		_FreeOSPage(0, IMG_TRUE, psPagePoolEntry->ppsPageArray[i]);
	}
	OSFreeMemNoStats(psPagePoolEntry->ppsPageArray);
	kmem_cache_free(g_psLinuxPagePoolCache, psPagePoolEntry);
	OSFreeMem(psCleanupData);

	OSAtomicDecrement(&g_iPoolCleanTasks);

	return PVRSRV_OK;
#endif /* defined(PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES) */
}


/* Put page array to the page pool.
 * Handles locking and checks whether the pages are
 * suitable to be stored in the pool. */
static inline IMG_BOOL
_PutPagesToPoolLocked(IMG_UINT32 ui32CPUCacheFlags,
					  struct page **ppsPageArray,
					  IMG_BOOL bUnpinned,
					  IMG_UINT32 uiOrder,
					  IMG_UINT32 uiNumPages)
{
	LinuxCleanupData *psCleanupData;
	PVRSRV_CLEANUP_THREAD_WORK *psCleanupThreadFn;
#if defined(SUPPORT_PHYSMEM_TEST)
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
#endif

	if (uiOrder == 0 &&
		!bUnpinned &&
		!PVRSRV_CHECK_CPU_CACHED(ui32CPUCacheFlags))
	{
		IMG_UINT32 uiEntries;
		IMG_UINT32 *puiCounter;
		struct list_head *psPoolHead;


		_PagePoolLock();

		uiEntries = _PagesInPoolUnlocked();

		/* Check for number of current page pool entries and whether
		 * we have other asynchronous tasks in-flight */
		if ( (uiEntries < g_ui32PagePoolMaxEntries) &&
		     ((uiEntries + uiNumPages) <
		      (g_ui32PagePoolMaxEntries + g_ui32PagePoolMaxExcessEntries) ))
		{
			if (OSAtomicIncrement(&g_iPoolCleanTasks) <=
					PVR_LINUX_PHYSMEM_MAX_ASYNC_CLEAN_TASKS)
			{
#if defined(SUPPORT_PHYSMEM_TEST)
				if (!psPVRSRVData->hCleanupThread)
				{
					goto eDecrement;
				}
#endif

				psCleanupData = OSAllocMem(sizeof(*psCleanupData));

				if (!psCleanupData)
				{
					PVR_DPF((PVR_DBG_ERROR,
							 "%s: Failed to get memory for deferred page pool cleanup. "
							 "Trying to free pages immediately",
							 __FUNCTION__));
					goto eDecrement;
				}

				psCleanupThreadFn = &psCleanupData->sCleanupWork;
				psCleanupData->ui32CPUCacheMode = ui32CPUCacheFlags;
				psCleanupData->psPoolEntry = kmem_cache_alloc(g_psLinuxPagePoolCache, GFP_KERNEL);

				if (!psCleanupData->psPoolEntry)
				{
					PVR_DPF((PVR_DBG_ERROR,
							 "%s: Failed to get memory for deferred page pool cleanup. "
							 "Trying to free pages immediately",
							 __FUNCTION__));
					goto eFreeCleanupData;
				}

				if (!_GetPoolListHead(ui32CPUCacheFlags, &psPoolHead, &puiCounter))
				{
					PVR_DPF((PVR_DBG_ERROR,
							 "%s: Failed to get correct page pool",
							 __FUNCTION__));
					goto eFreePoolEntry;
				}

				/* Increase counter here to avoid deferred cleanup tasks piling up */
				*puiCounter = *puiCounter + uiNumPages;

				psCleanupData->psPoolEntry->ppsPageArray = ppsPageArray;
				psCleanupData->psPoolEntry->uiItemsRemaining = uiNumPages;

				psCleanupThreadFn->pfnFree = _CleanupThread_CleanPages;
				psCleanupThreadFn->pvData = psCleanupData;
				psCleanupThreadFn->bDependsOnHW = IMG_FALSE;
				CLEANUP_THREAD_SET_RETRY_COUNT(psCleanupThreadFn,
				                               CLEANUP_THREAD_RETRY_COUNT_DEFAULT);

	#if defined(PVRSRV_ENABLE_PROCESS_STATS)
				/* MemStats usually relies on having the bridge lock held, however
				 * the page pool code may call PVRSRVStatsIncrMemAllocPoolStat and
				 * PVRSRVStatsDecrMemAllocPoolStat without the bridge lock held, so
				 * the page pool lock is used to ensure these calls are mutually
				 * exclusive
				 */
				PVRSRVStatsIncrMemAllocPoolStat(PAGE_SIZE * uiNumPages);
	#endif

				/* We must not hold the pool lock when calling AddWork because it might call us back to
				 * free pooled pages directly when unloading the driver	 */
				_PagePoolUnlock();

				PVRSRVCleanupThreadAddWork(psCleanupThreadFn);


			}
			else
			{
				goto eDecrement;
			}

		}
		else
		{
			goto eUnlock;
		}
	}
	else
	{
		goto eExitFalse;
	}

	return IMG_TRUE;

eFreePoolEntry:
	OSFreeMem(psCleanupData->psPoolEntry);
eFreeCleanupData:
	OSFreeMem(psCleanupData);
eDecrement:
	OSAtomicDecrement(&g_iPoolCleanTasks);
eUnlock:
	_PagePoolUnlock();
eExitFalse:
	return IMG_FALSE;
}

/* Get the GFP flags that we pass to the page allocator */
static inline gfp_t
_GetGFPFlags(IMG_BOOL bZero,
             PVRSRV_DEVICE_NODE *psDevNode)
{
	struct device *psDev = psDevNode->psDevConfig->pvOSDevice;
	gfp_t gfp_flags = GFP_USER | __GFP_NOWARN | __GFP_NOMEMALLOC;

#if defined(PVR_LINUX_PHYSMEM_USE_HIGHMEM_ONLY)
	/* Force use of HIGHMEM */
	gfp_flags |= __GFP_HIGHMEM;

	PVR_UNREFERENCED_PARAMETER(psDev);
#else

	if (psDev)
	{
		if (*psDev->dma_mask > DMA_BIT_MASK(32))
		{
			/* If our system is able to handle large addresses use highmem */
			gfp_flags |= __GFP_HIGHMEM;
		}
		else if (*psDev->dma_mask == DMA_BIT_MASK(32))
		{
			/* Limit to 32 bit.
			 * Achieved by NOT setting __GFP_HIGHMEM for 32 bit systems and
			 * setting __GFP_DMA32 for 64 bit systems */
#if defined(CONFIG_MACH_MT6739)
			/* change to use __GFP_HIGHMEM since __GFP_DMA32 can access only 512 MB DRAM */
			gfp_flags |= __GFP_HIGHMEM;
#else
			gfp_flags |= __GFP_DMA32;
#endif
		}
		else
		{
			/* Limit to size of DMA zone. */
			gfp_flags |= __GFP_DMA;
		}
	}
#endif

	if (bZero)
	{
		gfp_flags |= __GFP_ZERO;
	}

	return gfp_flags;
}

/*
 * @Function _PoisonDevicePage
 *
 * @Description  Poisons a device page. In normal case the device page has the
 *               same size as the OS page and so the ui32DevPageOrder will be
 *               equal to 0 and page argument will point to one OS page
 *               structure. In case of Non4K pages the order will be greater
 *               than 0 and page argument will point to an array of OS
 *               allocated pages.
 *
 * @Input psDevNode          pointer to the device object
 * @Input page               array of the pages allocated by from the OS
 * @Input ui32DevPageOrder   order of the page (same as the one used to allocate
 *                           the page array by alloc_pages())
 * @Input ui32CPUCacheFlags  CPU cache flags applied to the page
 * @Input ui8PoisonValue     value used to poison the page
 */
static void
_PoisonDevicePage(PVRSRV_DEVICE_NODE *psDevNode,
                  struct page *page,
                  IMG_UINT32 ui32DevPageOrder,
                  IMG_UINT32 ui32CPUCacheFlags,
                  IMG_BYTE ui8PoisonValue)
{
	IMG_CPU_PHYADDR sCPUPhysAddrStart, sCPUPhysAddrEnd;
	IMG_UINT32 ui32OsPageIdx;

	for (ui32OsPageIdx = 0;
	     ui32OsPageIdx < (1U << ui32DevPageOrder);
	     ui32OsPageIdx++)
	{
		struct page *current_page = page + ui32OsPageIdx;
		void *kvaddr = kmap(current_page);

		if (PVRSRV_CHECK_CPU_UNCACHED(ui32CPUCacheFlags) ||
		    PVRSRV_CHECK_CPU_WRITE_COMBINE(ui32CPUCacheFlags))
		{
			OSDeviceMemSet(kvaddr, ui8PoisonValue, PAGE_SIZE);
		}
		else
		{
			OSCachedMemSet(kvaddr, ui8PoisonValue, PAGE_SIZE);
		}

#if defined(CONFIG_L4)
		sCPUPhysAddrStart.uiAddr = l4x_virt_to_phys((void *)((phys_addr_t)page_to_pfn(page) << PAGE_SHIFT));
#else
		sCPUPhysAddrStart.uiAddr = page_to_phys(page);
#endif
		sCPUPhysAddrEnd.uiAddr = sCPUPhysAddrStart.uiAddr + PAGE_SIZE;

		OSCPUCacheFlushRangeKM(psDevNode,
		                       kvaddr, kvaddr + PAGE_SIZE,
		                       sCPUPhysAddrStart, sCPUPhysAddrEnd);

		kunmap(current_page);
	}
}

/* Allocate and initialise the structure to hold the metadata of the allocation */
static PVRSRV_ERROR
_AllocOSPageArray(PVRSRV_DEVICE_NODE *psDevNode,
				  PMR_SIZE_T uiChunkSize,
				  IMG_UINT32 ui32NumPhysChunks,
				  IMG_UINT32 ui32NumVirtChunks,
				  IMG_UINT32 uiLog2AllocPageSize,
				  IMG_BOOL bZero,
				  IMG_BOOL bIsCMA,
				  IMG_BOOL bPoisonOnAlloc,
				  IMG_BOOL bPoisonOnFree,
				  IMG_BOOL bOnDemand,
				  IMG_UINT32 ui32CPUCacheFlags,
				  IMG_PID uiPid,
				  PMR_OSPAGEARRAY_DATA **ppsPageArrayDataPtr)
{
	PVRSRV_ERROR eError;
	PMR_SIZE_T uiSize = uiChunkSize * ui32NumVirtChunks;
	IMG_UINT32 uiNumOSPageSizeVirtPages;
	IMG_UINT32 uiNumDevPageSizeVirtPages;
	PMR_OSPAGEARRAY_DATA *psPageArrayData;
	IMG_UINT64 ui64DmaMask = 0;
	PVR_UNREFERENCED_PARAMETER(ui32NumPhysChunks);

	/* Use of cast below is justified by the assertion that follows to
	 * prove that no significant bits have been truncated */
	uiNumOSPageSizeVirtPages = (IMG_UINT32) (((uiSize - 1) >> PAGE_SHIFT) + 1);
	PVR_ASSERT(((PMR_SIZE_T) uiNumOSPageSizeVirtPages << PAGE_SHIFT) == uiSize);

	uiNumDevPageSizeVirtPages = uiNumOSPageSizeVirtPages >> (uiLog2AllocPageSize - PAGE_SHIFT);

	/* Allocate the struct to hold the metadata */
	psPageArrayData = kmem_cache_alloc(g_psLinuxPageArray, GFP_KERNEL);
	if (psPageArrayData == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: OS refused the memory allocation for the private data.",
				 __func__));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e_freed_none;
	}

	/*
	 * Allocate the page array
	 *
	 * We avoid tracking this memory because this structure might go into the page pool.
	 * The OS can drain the pool asynchronously and when doing that we have to avoid
	 * any potential deadlocks.
	 *
	 * In one scenario the process stats vmalloc hash table lock is held and then
	 * the oom-killer softirq is trying to call _ScanObjectsInPagePool(), it must not
	 * try to acquire the vmalloc hash table lock again.
	 */
	psPageArrayData->pagearray = OSAllocZMemNoStats(sizeof(struct page *) * uiNumDevPageSizeVirtPages);
	if (psPageArrayData->pagearray == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e_free_kmem_cache;
	}
	else
	{
		if (bIsCMA)
		{
			/* Allocate additional DMA/CMA cpu kernel virtual address & device bus address array state */
			psPageArrayData->dmavirtarray = OSAllocZMemNoStats(sizeof(void*) * uiNumDevPageSizeVirtPages);
			if (psPageArrayData->dmavirtarray == NULL)
			{
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e_free_pagearray;
			}

			psPageArrayData->dmaphysarray = OSAllocZMemNoStats(sizeof(dma_addr_t) * uiNumDevPageSizeVirtPages);
			if (psPageArrayData->dmaphysarray == NULL)
			{
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e_free_cpuvirtaddrarray;
			}
		}
	}

	if (psDevNode->psDevConfig && psDevNode->psDevConfig->pvOSDevice)
	{
		struct device *psDev = psDevNode->psDevConfig->pvOSDevice;
		ui64DmaMask = *psDev->dma_mask;
	}

	/* Init metadata */
	psPageArrayData->psDevNode = psDevNode;
	psPageArrayData->uiPid = uiPid;
	psPageArrayData->iNumOSPagesAllocated = 0;
	psPageArrayData->uiTotalNumOSPages = uiNumOSPageSizeVirtPages;
	psPageArrayData->uiLog2AllocPageSize = uiLog2AllocPageSize;
	psPageArrayData->ui64DmaMask = ui64DmaMask;
	psPageArrayData->bZero = bZero;
	psPageArrayData->bIsCMA = bIsCMA;
	psPageArrayData->bOnDemand = bOnDemand;
	psPageArrayData->bUnpinned = IMG_FALSE;
	psPageArrayData->bPoisonOnFree = bPoisonOnFree;
	psPageArrayData->bPoisonOnAlloc = bPoisonOnAlloc;
	psPageArrayData->ui32CPUCacheFlags = ui32CPUCacheFlags;

	/* Indicate whether this is an allocation with default caching attribute (i.e cached) or not */
	if (PVRSRV_CHECK_CPU_UNCACHED(ui32CPUCacheFlags) ||
		PVRSRV_CHECK_CPU_WRITE_COMBINE(ui32CPUCacheFlags))
	{
		psPageArrayData->bUnsetMemoryType = IMG_TRUE;
	}
	else
	{
		psPageArrayData->bUnsetMemoryType = IMG_FALSE;
	}

	*ppsPageArrayDataPtr = psPageArrayData;
	return PVRSRV_OK;

/* Error path */
e_free_cpuvirtaddrarray:
	OSFreeMemNoStats(psPageArrayData->dmavirtarray);

e_free_pagearray:
	OSFreeMemNoStats(psPageArrayData->pagearray);

e_free_kmem_cache:
	kmem_cache_free(g_psLinuxPageArray, psPageArrayData);
	PVR_DPF((PVR_DBG_ERROR,
			 "%s: OS refused the memory allocation for the page pointer table. "
			 "Did you ask for too much?",
			 __func__));

e_freed_none:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

static inline void
_ApplyCacheMaintenance(PVRSRV_DEVICE_NODE *psDevNode,
					   struct page **ppsPage,
					   IMG_UINT32 uiNumPages,
					   IMG_BOOL bFlush)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_RETRY;
	void * pvAddr;


	//if ((uiNumPages << PAGE_SHIFT) >= PVR_DIRTY_BYTES_FLUSH_THRESHOLD)
	//{
		/* May fail so fallback to range-based flush */
		//eError = OSCPUOperation(PVRSRV_CACHE_OP_FLUSH);
	//}


	if (eError != PVRSRV_OK)
	{

		if (OSCPUCacheOpAddressType() == PVRSRV_CACHE_OP_ADDR_TYPE_VIRTUAL)
		{
			pgprot_t pgprot = PAGE_KERNEL;

			IMG_UINT32 uiNumToClean = uiNumPages;
			struct page **ppsCleanArray = ppsPage;

			/* Map and flush page.
			 * For large page arrays do it PVR_LINUX_PHYSMEM_MAX_KMAP_SIZE
			 * at a time. */
			while (uiNumToClean != 0)
			{
				IMG_UINT32 uiToClean = MIN(PVR_LINUX_PHYSMEM_MAX_KMAP_PAGES,
				                           uiNumToClean);
				IMG_CPU_PHYADDR sUnused =
					{ IMG_CAST_TO_CPUPHYADDR_UINT(0xCAFEF00DDEADBEEFULL) };

				pvAddr = vm_map_ram(ppsCleanArray, uiToClean, -1, pgprot);
				if (!pvAddr)
				{
					PVR_DPF((PVR_DBG_ERROR,
							"Unable to flush page cache for new allocation, skipping flush."));
					return;
				}

				CacheOpExec(psDevNode,
							pvAddr,
							pvAddr + PAGE_SIZE,
							sUnused,
							sUnused,
							PVRSRV_CACHE_OP_FLUSH);

				vm_unmap_ram(pvAddr, uiToClean);

				ppsCleanArray = &(ppsCleanArray[uiToClean]);
				uiNumToClean -= uiToClean;
			}
		}
		else
		{
			IMG_UINT32 ui32Idx;

			for (ui32Idx = 0; ui32Idx < uiNumPages; ++ui32Idx)
			{
				IMG_CPU_PHYADDR sCPUPhysAddrStart, sCPUPhysAddrEnd;

				pvAddr = kmap(ppsPage[ui32Idx]);
#if defined(CONFIG_L4)
				sCPUPhysAddrStart.uiAddr = l4x_virt_to_phys((void *)((phys_addr_t)page_to_pfn(ppsPage[ui32Idx]) << PAGE_SHIFT));
#else
				sCPUPhysAddrStart.uiAddr = page_to_phys(ppsPage[ui32Idx]);
#endif
				sCPUPhysAddrEnd.uiAddr = sCPUPhysAddrStart.uiAddr + PAGE_SIZE;

				/* If we're zeroing, we need to make sure the cleared memory is pushed out
				 * of the cache before the cache lines are invalidated */
				CacheOpExec(psDevNode,
							pvAddr,
							pvAddr + PAGE_SIZE,
							sCPUPhysAddrStart,
							sCPUPhysAddrEnd,
							PVRSRV_CACHE_OP_FLUSH);

				kunmap(ppsPage[ui32Idx]);
			}
		}

	}
}

/* Change the caching attribute of pages on x86 systems and takes care of
 * cache maintenance. This function is supposed to be called once for pages that
 * came from alloc_pages(). It expects an array of OS page sized pages!
 *
 * Flush/Invalidate pages in case the allocation is not cached. Necessary to
 * remove pages from the cache that might be flushed later and corrupt memory. */
static inline PVRSRV_ERROR
_ApplyOSPagesAttribute(PVRSRV_DEVICE_NODE *psDevNode,
					   struct page **ppsPage,
					   IMG_UINT32 uiNumPages,
					   IMG_BOOL bFlush,
					   IMG_UINT32 ui32CPUCacheFlags)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_BOOL bCPUCached = PVRSRV_CHECK_CPU_CACHED(ui32CPUCacheFlags);
	IMG_BOOL bCPUUncached = PVRSRV_CHECK_CPU_UNCACHED(ui32CPUCacheFlags);
	IMG_BOOL bCPUWriteCombine = PVRSRV_CHECK_CPU_WRITE_COMBINE(ui32CPUCacheFlags);

	if (ppsPage != NULL && uiNumPages != 0)
	{
#if defined (CONFIG_X86)
		/* On x86 we have to set page cache attributes for non-cached pages.
		 * The call is implicitly taking care of all flushing/invalidating
		 * and therefore we can skip the usual cache maintenance after this. */
		if (bCPUUncached || bCPUWriteCombine)
		{
			/* On x86 if we already have a mapping (e.g. low memory) we need to change the mode of
				current mapping before we map it ourselves	*/
			int ret = IMG_FALSE;
			PVR_UNREFERENCED_PARAMETER(bFlush);

			switch (PVRSRV_CPU_CACHE_MODE(ui32CPUCacheFlags))
			{
				case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
					ret = set_pages_array_uc(ppsPage, uiNumPages);
					if (ret)
					{
						eError = PVRSRV_ERROR_UNABLE_TO_SET_CACHE_MODE;
						PVR_DPF((PVR_DBG_ERROR, "Setting Linux page caching mode to UC failed, returned %d", ret));
					}
					break;

				case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
					ret = set_pages_array_wc(ppsPage, uiNumPages);
					if (ret)
					{
						eError = PVRSRV_ERROR_UNABLE_TO_SET_CACHE_MODE;
						PVR_DPF((PVR_DBG_ERROR, "Setting Linux page caching mode to WC failed, returned %d", ret));
					}
					break;

				case PVRSRV_MEMALLOCFLAG_CPU_CACHED:
					break;

				default:
					break;
			}
		}
		else
#endif
		{
			if ( bFlush ||
				 bCPUUncached || bCPUWriteCombine ||
				 (bCPUCached && PVRSRV_CHECK_CPU_CACHE_CLEAN(ui32CPUCacheFlags)) )
			{
				/*  We can be given pages which still remain in the cache.
					In order to make sure that the data we write through our mappings
					doesn't get overwritten by later cache evictions we invalidate the
					pages that are given to us.

					Note:
					This still seems to be true if we request cold pages, it's just less
					likely to be in the cache. */
				_ApplyCacheMaintenance(psDevNode,
									   ppsPage,
									   uiNumPages,
									   bFlush);
			}
		}
	}

	return eError;
}

/* Same as _AllocOSPage except it uses DMA framework to perform allocation.
 * uiPageIndex is expected to be the pagearray index where to store the higher order page. */
static PVRSRV_ERROR
_AllocOSPage_CMA(PMR_OSPAGEARRAY_DATA *psPageArrayData,
				gfp_t gfp_flags,
				IMG_UINT32 ui32AllocOrder,
				IMG_UINT32 ui32MinOrder,
				IMG_UINT32 uiPageIndex)
{
	void *virt_addr;
	struct page *page;
	dma_addr_t bus_addr;
	IMG_UINT32 uiAllocIsMisaligned;
	size_t alloc_size = PAGE_SIZE << ui32AllocOrder;
	struct device *dev = psPageArrayData->psDevNode->psDevConfig->pvOSDevice;
	PVR_ASSERT(ui32AllocOrder == ui32MinOrder);

	do
	{
		DisableOOMKiller();
#if defined(PVR_LINUX_PHYSMEM_SUPPRESS_DMA_AC)
		virt_addr = NULL;
#else
		virt_addr = dma_alloc_coherent(dev, alloc_size, &bus_addr, gfp_flags);
#endif
		if (virt_addr == NULL)
		{
			/* The idea here is primarily to support some older kernels with
			   broken or non-functioning DMA/CMA implementations (< Linux-3.4)
			   and to also handle DMA/CMA allocation failures by attempting a
			   normal page allocation though we expect dma_alloc_coherent()
			   already attempts this internally also before failing but
			   nonetheless it does no harm to retry the allocation ourselves */
			page = alloc_pages(gfp_flags, ui32AllocOrder);
			if (page)
			{
				/* Taint bus_addr as alloc_page, needed when freeing;
				   also acquire the low memory page address only, this
				   prevents mapping possible high memory pages into
				   kernel virtual address space which might exhaust
				   the VMALLOC address space */
				bus_addr = DMA_SET_ALLOCPG_ADDR(page_to_phys(page));
				virt_addr = (void*)(uintptr_t) DMA_VADDR_NOT_IN_USE;
			}
			else
			{
				return PVRSRV_ERROR_OUT_OF_MEMORY;
			}
		}
		else
		{
#if defined(CONFIG_L4)
			size_t size;
			for (size = 0; size < alloc_size; size += PAGE_SIZE)
			{
				/* In L4 prefault pages now, ensure compiler does not optimise this out */
				*((volatile int*)virt_addr + size) = *((volatile int*)virt_addr + size);
			}

			page = pfn_to_page((unsigned long)l4x_phys_to_virt(bus_addr) >> PAGE_SHIFT);
#else
#if !defined(CONFIG_ARM)
			page = pfn_to_page(bus_addr >> PAGE_SHIFT);
#else
			page = pfn_to_page(dma_to_pfn(dev, bus_addr));
#endif
#endif
		}
		EnableOOMKiller();

		/* Physical allocation alignment works/hidden behind the scene transparently,
		   we do this here if the allocated buffer address does not meet its alignment
		   requirement by over-allocating using the next power-2 order and reporting
		   aligned-adjusted values back to meet the requested alignment constraint.
		   Evidently we waste memory by doing this so should only do so if we do not
		   initially meet the alignment constraint. */
		uiAllocIsMisaligned = DMA_GET_ADDR(bus_addr) & ((PAGE_SIZE<<ui32MinOrder)-1);
		if (uiAllocIsMisaligned || ui32AllocOrder > ui32MinOrder)
		{
			IMG_BOOL bUsedAllocPages = DMA_IS_ALLOCPG_ADDR(bus_addr);
			if (ui32AllocOrder == ui32MinOrder)
			{
				if (bUsedAllocPages)
				{
					__free_pages(page, ui32AllocOrder);
				}
				else
				{
					dma_free_coherent(dev, alloc_size, virt_addr, bus_addr);
				}

				ui32AllocOrder = ui32AllocOrder + 1;
				alloc_size = PAGE_SIZE << ui32AllocOrder;

				PVR_ASSERT(uiAllocIsMisaligned != 0);
			}
			else
			{
				size_t align_adjust = PAGE_SIZE << ui32MinOrder;

				/* Adjust virtual/bus addresses to meet alignment */
				bus_addr = !bUsedAllocPages ? bus_addr :
#if defined(CONFIG_L4)
							l4x_virt_to_phys((void *)((phys_addr_t)page_to_pfn(page) << PAGE_SHIFT));
#else
							page_to_phys(page);
#endif
				align_adjust = PVR_ALIGN((size_t)bus_addr, align_adjust);
				align_adjust -= (size_t)bus_addr;

				if (align_adjust)
				{
					if (bUsedAllocPages)
					{
						page += align_adjust >> PAGE_SHIFT;
						bus_addr = DMA_SET_ALLOCPG_ADDR(page_to_phys(page));
						virt_addr = (void*)(uintptr_t) DMA_VADDR_NOT_IN_USE;
					}
					else
					{
#if defined(CONFIG_L4)
						size_t size;
						for (size = 0; size < alloc_size; size += PAGE_SIZE)
						{
							/* In L4 prefault pages now, ensure compiler does not optimise this out */
							*((volatile int*)virt_addr + size) = *((volatile int*)virt_addr + size);
						}
#endif
						bus_addr += align_adjust;
						virt_addr += align_adjust;
#if defined(CONFIG_L4)
						page = pfn_to_page((unsigned long)l4x_phys_to_virt(bus_addr) >> PAGE_SHIFT);
#else
#if !defined(CONFIG_ARM)
						page = pfn_to_page(bus_addr >> PAGE_SHIFT);
#else
						page = pfn_to_page(dma_to_pfn(dev, bus_addr));
#endif
#endif
					}

					/* Store adjustments in PAGE_SIZE counts */
					align_adjust = align_adjust >> PAGE_SHIFT;
					bus_addr = DMA_SET_ALIGN_ADJUSTMENT(bus_addr, align_adjust);
				}

				/* Taint bus_addr due to over-allocation, allows us to free
				 * memory correctly */
				bus_addr = DMA_SET_ADJUSTED_ADDR(bus_addr);
				uiAllocIsMisaligned = 0;
			}
		}
	} while (uiAllocIsMisaligned);

	/* Convert OSPageSize-based index into DevicePageSize-based index */
	psPageArrayData->dmavirtarray[uiPageIndex] = virt_addr;
	psPageArrayData->dmaphysarray[uiPageIndex] = bus_addr;
	psPageArrayData->pagearray[uiPageIndex] = page;

	return PVRSRV_OK;
}

/* Allocate a page of order uiAllocOrder and stores it in the page array ppsPage at
 * position uiPageIndex.
 *
 * If the order is higher than 0, it splits the page into multiples and
 * stores them at position uiPageIndex to uiPageIndex+(1<<uiAllocOrder).
 *
 * This function is supposed to be used for uiMinOrder == 0 only! */
static PVRSRV_ERROR
_AllocOSPage(PMR_OSPAGEARRAY_DATA *psPageArrayData,
			gfp_t gfp_flags,
			IMG_UINT32 uiAllocOrder,
			IMG_UINT32 uiMinOrder,
			IMG_UINT32 uiPageIndex)
{
	struct page *psPage;
	IMG_UINT32 ui32Count;

	/* Sanity check. If it fails we write into the wrong places in the array. */
	PVR_ASSERT(uiMinOrder == 0);

	/* Allocate the page */
	DisableOOMKiller();
	psPage = alloc_pages(gfp_flags, uiAllocOrder);
	EnableOOMKiller();

	if (psPage == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	/* In case we need to, split the higher order page;
	   this should only be used for order-0 allocations
	   as higher order allocations should use DMA/CMA */
	if (uiAllocOrder != 0)
	{
		split_page(psPage, uiAllocOrder);
	}
#endif

	/* Store the page (or multiple split pages) in the page array */
	for (ui32Count = 0; ui32Count < (1 << uiAllocOrder); ui32Count++)
	{
		psPageArrayData->pagearray[uiPageIndex + ui32Count] = &(psPage[ui32Count]);
	}

	return PVRSRV_OK;
}

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if defined(PVRSRV_ENABLE_MEMORY_STATS)

static inline void _AddMemAllocRecord_UmaPages(PMR_OSPAGEARRAY_DATA *psPageArrayData,
                                               struct page *psPage)
{
	IMG_CPU_PHYADDR sCPUPhysAddr = { page_to_phys(psPage) };
	PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES,
	                             NULL, sCPUPhysAddr,
	                             1 << psPageArrayData->uiLog2AllocPageSize,
	                             NULL, psPageArrayData->uiPid);
}

static inline void _RemoveMemAllocRecord_UmaPages(PMR_OSPAGEARRAY_DATA *psPageArrayData,
                                                  struct page *psPage)
{
	PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES,
	                                (IMG_UINT64) page_to_phys(psPage),
	                                psPageArrayData->uiPid);
}

#else /* defined(PVRSRV_ENABLE_MEMORY_STATS) */

static inline void _IncrMemAllocStat_UmaPages(size_t uiSize, IMG_PID uiPid)
{
	PVRSRVStatsIncrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES,
	                            uiSize, uiPid);
}

static inline void _DecrMemAllocStat_UmaPages(size_t uiSize, IMG_PID uiPid)
{
	PVRSRVStatsDecrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_ALLOC_UMA_PAGES,
	                            uiSize, uiPid);
}

#endif /* defined(PVRSRV_ENABLE_MEMORY_STATS) */
#endif /* defined(PVRSRV_ENABLE_PROCESS_STATS) */

/* Allocation of OS pages: We may allocate 2^N order pages at a time for two reasons.
 *
 * Firstly to support device pages which are larger than OS. By asking the OS for 2^N
 * order OS pages at a time we guarantee the device page is contiguous.
 *
 * Secondly for performance where we may ask for 2^N order pages to reduce the number
 * of calls to alloc_pages, and thus reduce time for huge allocations.
 *
 * Regardless of page order requested, we need to break them down to track _OS pages.
 * The maximum order requested is increased if all max order allocations were successful.
 * If any request fails we reduce the max order.
 */
static PVRSRV_ERROR
_AllocOSPages_Fast(PMR_OSPAGEARRAY_DATA *psPageArrayData)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 uiArrayIndex = 0;
	IMG_UINT32 ui32Order;
	IMG_UINT32 ui32MinOrder = psPageArrayData->uiLog2AllocPageSize - PAGE_SHIFT;
	IMG_BOOL bIncreaseMaxOrder = IMG_TRUE;

	IMG_UINT32 ui32NumPageReq;
	IMG_UINT32 uiPagesToAlloc;
	IMG_UINT32 uiPagesFromPool = 0;

	gfp_t gfp_flags = _GetGFPFlags(ui32MinOrder ? psPageArrayData->bZero : IMG_FALSE, /* Zero all pages later as batch */
	                                      psPageArrayData->psDevNode);
	gfp_t ui32GfpFlags;
	gfp_t ui32HighOrderGfpFlags = ((gfp_flags & ~__GFP_RECLAIM) | __GFP_NORETRY);

	struct page **ppsPageArray = psPageArrayData->pagearray;
	struct page **ppsPageAttributeArray = NULL;

	uiPagesToAlloc = psPageArrayData->uiTotalNumOSPages;

	/* Try to get pages from the pool since it is faster;
	   the page pool currently only supports zero-order pages
	   thus currently excludes all DMA/CMA allocated memory */
	_GetPagesFromPoolLocked(psPageArrayData->psDevNode,
							psPageArrayData->ui32CPUCacheFlags,
							uiPagesToAlloc,
							ui32MinOrder,
							psPageArrayData->bZero,
							ppsPageArray,
							&uiPagesFromPool);

	uiArrayIndex = uiPagesFromPool;

	if ((uiPagesToAlloc - uiPagesFromPool) < PVR_LINUX_HIGHORDER_ALLOCATION_THRESHOLD)
	{	/* Small allocations: ask for one device page at a time */
		ui32Order = ui32MinOrder;
		bIncreaseMaxOrder = IMG_FALSE;
	}
	else
	{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
		/* Large zero-order or none zero-order allocations, ask for
		   MAX(max-order,min-order) order pages at a time; alloc
		   failures throttles this down to ZeroOrder allocations */
		ui32Order = MAX(g_uiMaxOrder, ui32MinOrder);
#else
		/* Because split_pages() is not available on older kernels
		   we cannot mix-and-match any-order pages in the PMR;
		   only same-order pages must be present in page array.
		   So we unconditionally force it to use ui32MinOrder on
		   these older kernels */
		ui32Order = ui32MinOrder;
#endif
	}

	/* Only if asking for more contiguity than we actually need, let it fail */
	ui32GfpFlags = (ui32Order > ui32MinOrder) ? ui32HighOrderGfpFlags : gfp_flags;
	ui32NumPageReq = (1 << ui32Order);

	while (uiArrayIndex < uiPagesToAlloc)
	{
		IMG_UINT32 ui32PageRemain = uiPagesToAlloc - uiArrayIndex;

		while (ui32NumPageReq > ui32PageRemain)
		{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
			/* Pages to request is larger than that remaining
			   so ask for less so never over allocate */
			ui32Order = MAX(ui32Order >> 1,ui32MinOrder);
#else
			/* Pages to request is larger than that remaining so
			   do nothing thus over allocate as we do not support
			   mix/match of any-order pages in PMR page-array in
			   older kernels (simplifies page free logic) */
			PVR_ASSERT(ui32Order == ui32MinOrder);
#endif
			ui32NumPageReq = (1 << ui32Order);
			ui32GfpFlags = (ui32Order > ui32MinOrder) ? ui32HighOrderGfpFlags : gfp_flags;
		}

		if (psPageArrayData->bIsCMA)
		{
			/* As the DMA/CMA framework rounds-up request to the
			   next power-of-two, we request multiple uiMinOrder
			   pages to satisfy allocation request in order to
			   minimise wasting memory */
			eError =  _AllocOSPage_CMA(psPageArrayData,
									   ui32GfpFlags,
									   ui32Order,
									   ui32MinOrder,
									   uiArrayIndex >> ui32MinOrder);
		}
		else
		{
			/* Allocate uiOrder pages at uiArrayIndex */
			eError = _AllocOSPage(psPageArrayData,
								  ui32GfpFlags,
								  ui32Order,
								  ui32MinOrder,
								  uiArrayIndex);
		}

		if (eError == PVRSRV_OK)
		{
			/* Successful request. Move onto next. */
			uiArrayIndex += ui32NumPageReq;
		}
		else
		{
			if (ui32Order > ui32MinOrder)
			{
				/* Last request failed. Let's ask for less next time */
				ui32Order = MAX(ui32Order >> 1,ui32MinOrder);
				bIncreaseMaxOrder = IMG_FALSE;
				ui32NumPageReq = (1 << ui32Order);
				ui32GfpFlags = (ui32Order > ui32MinOrder) ? ui32HighOrderGfpFlags : gfp_flags;
				g_uiMaxOrder = ui32Order;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
				/* We should not trigger this code path in older kernels,
				   this is enforced by ensuring ui32Order == ui32MinOrder */
				PVR_ASSERT(ui32Order == ui32MinOrder);
#endif
			}
			else
			{
				/* Failed to alloc pages at required contiguity. Failed allocation */
				PVR_DPF((PVR_DBG_ERROR, "%s: %s failed to honour request at %u of %u, flags = %x, order = %u (%s)",
								__FUNCTION__,
								psPageArrayData->bIsCMA ? "dma_alloc_coherent" : "alloc_pages",
								uiArrayIndex,
								uiPagesToAlloc,
								ui32GfpFlags,
								ui32Order,
								PVRSRVGetErrorStringKM(eError)));
				eError = PVRSRV_ERROR_PMR_FAILED_TO_ALLOC_PAGES;
				goto e_free_pages;
			}
		}
	}

	if (bIncreaseMaxOrder && (g_uiMaxOrder < PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM))
	{	/* All successful allocations on max order. Let's ask for more next time */
		g_uiMaxOrder++;
	}

	/* Construct table of page pointers to apply attributes */
	ppsPageAttributeArray = &ppsPageArray[uiPagesFromPool];
	if (psPageArrayData->bIsCMA)
	{
		IMG_UINT32 uiIdx, uiIdy, uiIdz;

		ppsPageAttributeArray = OSAllocMem(sizeof(struct page *) * uiPagesToAlloc);
		if (ppsPageAttributeArray == NULL)
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed OSAllocMem() for page attributes table"));
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto e_free_pages;
		}

		for (uiIdx = 0; uiIdx < uiPagesToAlloc; uiIdx += ui32NumPageReq)
		{
			uiIdy = uiIdx >> ui32Order;
			for (uiIdz = 0; uiIdz < ui32NumPageReq; uiIdz++)
			{
				ppsPageAttributeArray[uiIdx+uiIdz] = psPageArrayData->pagearray[uiIdy];
				ppsPageAttributeArray[uiIdx+uiIdz] += uiIdz;
			}
		}
	}

	if (psPageArrayData->bZero && ui32MinOrder == 0)
	{
		eError = _ZeroPageArray(uiPagesToAlloc - uiPagesFromPool,
					   ppsPageAttributeArray,
					   PAGE_KERNEL);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to zero pages (fast)"));
			goto e_free_pages;
		}
	}


	/* Do the cache management as required */
	eError = _ApplyOSPagesAttribute(psPageArrayData->psDevNode,
									ppsPageAttributeArray,
									uiPagesToAlloc - uiPagesFromPool,
									psPageArrayData->bZero,
									psPageArrayData->ui32CPUCacheFlags);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to set page attributes"));
		goto e_free_pages;
	}
	else
	{
		if (psPageArrayData->bIsCMA)
		{
			OSFreeMem(ppsPageAttributeArray);
		}
	}

	/* Update metadata */
	psPageArrayData->iNumOSPagesAllocated = psPageArrayData->uiTotalNumOSPages;

	{
		IMG_UINT32 ui32NumPages =
		            psPageArrayData->iNumOSPagesAllocated >> ui32MinOrder;
		IMG_UINT32 i;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
		for (i = 0; i < ui32NumPages; i++)
		{
			_AddMemAllocRecord_UmaPages(psPageArrayData, ppsPageArray[i]);
		}
#else
		_IncrMemAllocStat_UmaPages(uiPagesToAlloc * PAGE_SIZE,
		                               psPageArrayData->uiPid);
#endif
#endif

		if (psPageArrayData->bPoisonOnAlloc)
		{
			for (i = 0; i < ui32NumPages; i++)
			{
				_PoisonDevicePage(psPageArrayData->psDevNode,
				                  ppsPageArray[i],
				                  ui32Order,
				                  psPageArrayData->ui32CPUCacheFlags,
				                  PVRSRV_POISON_ON_ALLOC_VALUE);
			}
		}
	}

	return PVRSRV_OK;

/* Error path */
e_free_pages:
	{
		IMG_UINT32 ui32PageToFree;

		if (psPageArrayData->bIsCMA)
		{
			IMG_UINT32 uiDevArrayIndex = uiArrayIndex >> ui32Order;
			IMG_UINT32 uiDevPageSize = PAGE_SIZE << ui32Order;
			PVR_ASSERT(ui32Order == ui32MinOrder);

			if (ppsPageAttributeArray)
			{
				OSFreeMem(ppsPageAttributeArray);
			}

			for (ui32PageToFree = 0; ui32PageToFree < uiDevArrayIndex; ui32PageToFree++)
			{
				_FreeOSPage_CMA(psPageArrayData->psDevNode->psDevConfig->pvOSDevice,
								uiDevPageSize,
								ui32MinOrder,
								psPageArrayData->dmavirtarray[ui32PageToFree],
								psPageArrayData->dmaphysarray[ui32PageToFree],
								ppsPageArray[ui32PageToFree]);
				psPageArrayData->dmaphysarray[ui32PageToFree]= (dma_addr_t)0;
				psPageArrayData->dmavirtarray[ui32PageToFree] = NULL;
				ppsPageArray[ui32PageToFree] = NULL;
			}
		}
		else
		{
			/* Free the pages we got from the pool */
			for(ui32PageToFree = 0; ui32PageToFree < uiPagesFromPool; ui32PageToFree++)
			{
				_FreeOSPage(ui32MinOrder,
							psPageArrayData->bUnsetMemoryType,
							ppsPageArray[ui32PageToFree]);
				ppsPageArray[ui32PageToFree] = NULL;
			}

			for (ui32PageToFree = uiPagesFromPool; ui32PageToFree < uiArrayIndex; ui32PageToFree++)
			{
				_FreeOSPage(ui32MinOrder, IMG_FALSE, ppsPageArray[ui32PageToFree]);
				ppsPageArray[ui32PageToFree] = NULL;
			}
		}

		return eError;
	}
}

/* Allocation of OS pages: This function is used for sparse allocations.
 *
 * Sparse allocations provide only a proportion of sparse physical backing within the total
 * virtual range. */
static PVRSRV_ERROR
_AllocOSPages_Sparse(PMR_OSPAGEARRAY_DATA *psPageArrayData,
					 IMG_UINT32 *puiAllocIndices,
					 IMG_UINT32 uiPagesToAlloc)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 i;
	struct page **ppsPageArray = psPageArrayData->pagearray;
	IMG_UINT32 uiOrder = psPageArrayData->uiLog2AllocPageSize - PAGE_SHIFT;
	IMG_UINT32 uiPagesFromPool = 0;
	IMG_UINT32 uiNumOSPagesToAlloc = uiPagesToAlloc * (1 << uiOrder);
	IMG_UINT32 uiTotalNumAllocPages = psPageArrayData->uiTotalNumOSPages >> uiOrder;
	gfp_t ui32GfpFlags = _GetGFPFlags(uiOrder ? psPageArrayData->bZero :
									  IMG_FALSE, /* Zero pages later as batch */
									  psPageArrayData->psDevNode);

	 /* We use this page array to receive pages from the pool and then reuse it afterwards to
	  * store pages that need their cache attribute changed on x86*/
	struct page **ppsTempPageArray;
	IMG_UINT32 uiTempPageArrayIndex = 0;

	/* Allocate the temporary page array that we need here to receive pages
	 * from the pool and to store pages that need their caching attributes changed.
	 * Allocate number of OS pages to be able to use the attribute function later. */
	ppsTempPageArray = OSAllocMem(sizeof(struct page*) * uiNumOSPagesToAlloc);
	if (ppsTempPageArray == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed metadata allocation", __FUNCTION__));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e_exit;
	}

	/* Check the requested number of pages if they fit in the page array */
	if (uiTotalNumAllocPages <
			((psPageArrayData->iNumOSPagesAllocated >> uiOrder) + uiPagesToAlloc) )
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Trying to allocate more pages (Order %u) than this buffer can handle, "
				 "Request + Allocated < Max! Request %u, Allocated %u, Max %u.",
				 __FUNCTION__,
				 uiOrder,
				 uiPagesToAlloc,
				 psPageArrayData->iNumOSPagesAllocated >> uiOrder,
				 uiTotalNumAllocPages));
		eError = PVRSRV_ERROR_PMR_BAD_MAPPINGTABLE_SIZE;
		goto e_free_temp_array;
	}

	/* Try to get pages from the pool since it is faster */
	_GetPagesFromPoolLocked(psPageArrayData->psDevNode,
							psPageArrayData->ui32CPUCacheFlags,
							uiPagesToAlloc,
							uiOrder,
							psPageArrayData->bZero,
							ppsTempPageArray,
							&uiPagesFromPool);

	/* Allocate pages from the OS or move the pages that we got from the pool
	 * to the page array */
	for (i = 0; i < uiPagesToAlloc; i++)
	{
		/* Check if the indices we are allocating are in range */
		if (puiAllocIndices[i] >= uiTotalNumAllocPages)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Given alloc index %u at %u is larger than page array %u.",
					 __FUNCTION__,
					 i,
					 puiAllocIndices[i],
					 uiTotalNumAllocPages));
			eError = PVRSRV_ERROR_DEVICEMEM_OUT_OF_RANGE;
			goto e_free_pages;
		}

		/* Check if there is not already a page allocated at this position */
		if (NULL != ppsPageArray[puiAllocIndices[i]])
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Mapping number %u at page array index %u already exists. "
					 "Page struct %p",
					 __func__,
					 i,
					 puiAllocIndices[i],
					 ppsPageArray[puiAllocIndices[i]]));
			eError = PVRSRV_ERROR_PMR_MAPPING_ALREADY_EXISTS;
			goto e_free_pages;
		}

		/* Finally assign a page to the array.
		 * Either from the pool or allocate a new one. */
		if (uiPagesFromPool != 0)
		{
			uiPagesFromPool--;
			ppsPageArray[puiAllocIndices[i]] = ppsTempPageArray[uiPagesFromPool];
		}
		else
		{
			if (psPageArrayData->bIsCMA)
			{

				/* As the DMA/CMA framework rounds-up request to the
				   next power-of-two, we request multiple uiMinOrder
				   pages to satisfy allocation request in order to
				   minimise wasting memory */
				eError =  _AllocOSPage_CMA(psPageArrayData,
										   ui32GfpFlags,
										   uiOrder,
										   uiOrder,
										   puiAllocIndices[i]);
				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "Failed to alloc CMA pages"));
					goto e_free_pages;
				}
			}
			else
			{
				DisableOOMKiller();
				ppsPageArray[puiAllocIndices[i]] = alloc_pages(ui32GfpFlags, uiOrder);
				EnableOOMKiller();
			}

			if (ppsPageArray[puiAllocIndices[i]] != NULL)
			{
				/* Reusing the temp page array if it has no pool pages anymore */

				if (psPageArrayData->bIsCMA)
				{
					IMG_UINT32 idx;
					struct page* psPageAddr;

					psPageAddr = ppsPageArray[puiAllocIndices[i]];

					for (idx = 0; idx < (1 << uiOrder); idx++)
					{
						ppsTempPageArray[uiTempPageArrayIndex + idx] = psPageAddr;
						psPageAddr++;
					}
					uiTempPageArrayIndex += (1 << uiOrder);
				}
				else
				{
					ppsTempPageArray[uiTempPageArrayIndex] = ppsPageArray[puiAllocIndices[i]];
					uiTempPageArrayIndex++;
				}
			}
			else
			{
				/* Failed to alloc pages at required contiguity. Failed allocation */
				PVR_DPF((PVR_DBG_ERROR,
						 "%s: alloc_pages failed to honour request at %u of %u, flags = %x, order = %u",
						 __FUNCTION__,
						 i,
						 uiPagesToAlloc,
						 ui32GfpFlags,
						 uiOrder));
				eError = PVRSRV_ERROR_PMR_FAILED_TO_ALLOC_PAGES;
				goto e_free_pages;
			}
		}
	}

	if (psPageArrayData->bZero && uiOrder == 0)
	{
		eError = _ZeroPageArray(uiTempPageArrayIndex,
		                        ppsTempPageArray,
		                        PAGE_KERNEL);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to zero pages (sparse)"));
			goto e_free_pages;
		}
	}

	/* Do the cache management as required */
	eError = _ApplyOSPagesAttribute(psPageArrayData->psDevNode,
									ppsTempPageArray,
									uiTempPageArrayIndex,
									psPageArrayData->bZero,
									psPageArrayData->ui32CPUCacheFlags);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to set page attributes"));
		goto e_free_pages;
	}

	/* Update metadata */
	psPageArrayData->iNumOSPagesAllocated += uiNumOSPagesToAlloc;

	/* Free temporary page array */
	OSFreeMem(ppsTempPageArray);

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	for (i = 0; i < uiPagesToAlloc; i++)
	{
		_AddMemAllocRecord_UmaPages(psPageArrayData,
		                            ppsPageArray[puiAllocIndices[i]]);
	}
#else
	_IncrMemAllocStat_UmaPages(uiNumOSPagesToAlloc * PAGE_SIZE,
	                           psPageArrayData->uiPid);
#endif
#endif

	if (psPageArrayData->bPoisonOnAlloc)
	{
		for (i = 0; i < uiPagesToAlloc; i++)
		{
			_PoisonDevicePage(psPageArrayData->psDevNode,
			                  ppsPageArray[puiAllocIndices[i]],
			                  uiOrder,
			                  psPageArrayData->ui32CPUCacheFlags,
			                  PVRSRV_POISON_ON_ALLOC_VALUE);
		}
	}

	return PVRSRV_OK;

/* Error path */
e_free_pages:
	{
		IMG_UINT32 ui32PageToFree;

		if (psPageArrayData->bIsCMA)
		{
			IMG_UINT32 uiDevPageSize = PAGE_SIZE << uiOrder;

			for (ui32PageToFree = 0; ui32PageToFree < i; ui32PageToFree++)
			{
				_FreeOSPage_CMA(psPageArrayData->psDevNode->psDevConfig->pvOSDevice,
								uiDevPageSize,
								uiOrder,
								psPageArrayData->dmavirtarray[puiAllocIndices[ui32PageToFree]],
								psPageArrayData->dmaphysarray[puiAllocIndices[ui32PageToFree]],
								ppsPageArray[puiAllocIndices[ui32PageToFree]]);
				psPageArrayData->dmaphysarray[puiAllocIndices[ui32PageToFree]]= (dma_addr_t)0;
				psPageArrayData->dmavirtarray[puiAllocIndices[ui32PageToFree]] = NULL;
				ppsPageArray[puiAllocIndices[ui32PageToFree]] = NULL;
			}
		}
		else
		{
			/* Free the pages we got from the pool */
			for(ui32PageToFree = 0; ui32PageToFree < uiPagesFromPool; ui32PageToFree++)
			{
				_FreeOSPage(0,
							psPageArrayData->bUnsetMemoryType,
							ppsTempPageArray[ui32PageToFree]);
			}

			/* Free the pages we just allocated from the OS */
			for(ui32PageToFree = uiPagesFromPool; ui32PageToFree < i; ui32PageToFree++)
			{
				_FreeOSPage(0,
							IMG_FALSE,
							ppsPageArray[puiAllocIndices[ui32PageToFree]]);
			}

			/* Reset all page array entries that have been set so far*/
			for(ui32PageToFree = 0; ui32PageToFree < i; ui32PageToFree++)
			{
				ppsPageArray[puiAllocIndices[ui32PageToFree]] = NULL;
			}
		}
	}

e_free_temp_array:
	OSFreeMem(ppsTempPageArray);

e_exit:
	return eError;
}

/* Allocate pages for a given page array.
 *
 * The executed allocation path depends whether an array with allocation
 * indices has been passed or not */
static PVRSRV_ERROR
_AllocOSPages(PMR_OSPAGEARRAY_DATA *psPageArrayData,
			  IMG_UINT32 *puiAllocIndices,
			  IMG_UINT32 uiPagesToAlloc)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 i;
	struct page **ppsPageArray;

	/* Sanity checks */
	PVR_ASSERT(NULL != psPageArrayData);
	if (psPageArrayData->bIsCMA)
	{
		PVR_ASSERT(psPageArrayData->dmaphysarray != NULL);
		PVR_ASSERT(psPageArrayData->dmavirtarray != NULL);
	}
	PVR_ASSERT(psPageArrayData->pagearray != NULL);
	PVR_ASSERT(0 <= psPageArrayData->iNumOSPagesAllocated);

	ppsPageArray = psPageArrayData->pagearray;

	/* Go the sparse alloc path if we have an array with alloc indices.*/
	if (puiAllocIndices != NULL)
	{
		eError =  _AllocOSPages_Sparse(psPageArrayData,
									   puiAllocIndices,
									   uiPagesToAlloc);
	}
	else
	{
		eError =  _AllocOSPages_Fast(psPageArrayData);
	}

	if (eError != PVRSRV_OK)
	{
		goto e_exit;
	}

	_DumpPageArray(ppsPageArray,
	               psPageArrayData->uiTotalNumOSPages >>
					   (psPageArrayData->uiLog2AllocPageSize - PAGE_SHIFT) );


	for (i = 0; i < uiPagesToAlloc; i++) {
		IMG_CPU_PHYADDR sCPUPhysAddr;
		IMG_UINT32 uiIdx = puiAllocIndices ? puiAllocIndices[i] : i;

		sCPUPhysAddr.uiAddr = page_to_phys(ppsPageArray[uiIdx]);
		if ((sCPUPhysAddr.uiAddr > 0x1c000ffff) ||
			(sCPUPhysAddr.uiAddr < 0x40000000))
			ged_log_buf_print2(_mpu_ged_log, GED_LOG_ATTR_TIME,
			"sCPUPhysAddr.uiAddr=0x%llx out of range"
			, sCPUPhysAddr.uiAddr);
	}

	PVR_DPF((PVR_DBG_MESSAGE, "physmem_osmem_linux.c: allocated OS memory for PMR @0x%p", psPageArrayData));
	return PVRSRV_OK;

e_exit:
	return eError;
}

/* Same as _FreeOSPage except free memory using DMA framework */
static INLINE void
_FreeOSPage_CMA(struct device *dev,
				size_t alloc_size,
				IMG_UINT32 uiOrder,
				void *virt_addr,
				dma_addr_t dev_addr,
				struct page *psPage)
{
	if (DMA_IS_ALLOCPG_ADDR(dev_addr))
	{
#if defined(CONFIG_X86)
		void *pvPageVAddr = page_address(psPage);
		if (pvPageVAddr)
		{
			int ret = set_memory_wb((unsigned long)pvPageVAddr, 1);
			if (ret)
			{
				PVR_DPF((PVR_DBG_ERROR,
						"%s: Failed to reset page attribute",
						__FUNCTION__));
			}
		}
#endif

		if (DMA_IS_ADDR_ADJUSTED(dev_addr))
		{
			psPage -= DMA_GET_ALIGN_ADJUSTMENT(dev_addr);
			uiOrder += 1;
		}

		__free_pages(psPage, uiOrder);
	}
	else
	{
		if (DMA_IS_ADDR_ADJUSTED(dev_addr))
		{
			size_t align_adjust;

			align_adjust = DMA_GET_ALIGN_ADJUSTMENT(dev_addr);
			alloc_size = alloc_size << 1;

			dev_addr = DMA_GET_ADDR(dev_addr);
			dev_addr -= align_adjust << PAGE_SHIFT;
			virt_addr -= align_adjust << PAGE_SHIFT;
		}

		dma_free_coherent(dev, alloc_size, virt_addr, DMA_GET_ADDR(dev_addr));
	}
}

/* Free a single page back to the OS.
 * Make sure the cache type is set back to the default value.
 *
 * Note:
 * We must _only_ check bUnsetMemoryType in the case where we need to free
 * the page back to the OS since we may have to revert the cache properties
 * of the page to the default as given by the OS when it was allocated. */
static void
_FreeOSPage(IMG_UINT32 uiOrder,
			IMG_BOOL bUnsetMemoryType,
			struct page *psPage)
{

#if defined(CONFIG_X86)
	void *pvPageVAddr;
	pvPageVAddr = page_address(psPage);

	if (pvPageVAddr && bUnsetMemoryType == IMG_TRUE)
	{
		int ret;

		ret = set_memory_wb((unsigned long)pvPageVAddr, 1);
		if (ret)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to reset page attribute", __FUNCTION__));
		}
	}
#else
	PVR_UNREFERENCED_PARAMETER(bUnsetMemoryType);
#endif
	__free_pages(psPage, uiOrder);
}

/* Free the struct holding the metadata */
static PVRSRV_ERROR
_FreeOSPagesArray(PMR_OSPAGEARRAY_DATA *psPageArrayData)
{
	PVR_DPF((PVR_DBG_MESSAGE, "physmem_osmem_linux.c: freed OS memory for PMR @0x%p", psPageArrayData));

	/* Check if the page array actually still exists.
	 * It might be the case that has been moved to the page pool */
	if (psPageArrayData->pagearray != NULL)
	{
		OSFreeMemNoStats(psPageArrayData->pagearray);
	}

	kmem_cache_free(g_psLinuxPageArray, psPageArrayData);

	return PVRSRV_OK;
}

/* Free all or some pages from a sparse page array */
static PVRSRV_ERROR
_FreeOSPages_Sparse(PMR_OSPAGEARRAY_DATA *psPageArrayData,
					IMG_UINT32 *pai32FreeIndices,
					IMG_UINT32 ui32FreePageCount)
{
	IMG_BOOL bSuccess;
	IMG_UINT32 uiOrder = psPageArrayData->uiLog2AllocPageSize - PAGE_SHIFT;
	IMG_UINT32 uiPageIndex, i, j, uiTempIdx = 0;
	struct page **ppsPageArray = psPageArrayData->pagearray;
	IMG_UINT32 uiNumPages;

	struct page **ppsTempPageArray;
	IMG_UINT32 uiTempArraySize;

	/* We really should have something to free before we call this */
	PVR_ASSERT(psPageArrayData->iNumOSPagesAllocated != 0);

	if (pai32FreeIndices == NULL)
	{
		uiNumPages = psPageArrayData->uiTotalNumOSPages >> uiOrder;
		uiTempArraySize = psPageArrayData->iNumOSPagesAllocated;
	}
	else
	{
		uiNumPages = ui32FreePageCount;
		uiTempArraySize = ui32FreePageCount << uiOrder;
	}

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	for (i = 0; i < uiNumPages; i++)
	{
		IMG_UINT32 idx = pai32FreeIndices ? pai32FreeIndices[i] : i;
		_RemoveMemAllocRecord_UmaPages(psPageArrayData, ppsPageArray[idx]);
	}
#else
	_DecrMemAllocStat_UmaPages(uiTempArraySize * PAGE_SIZE,
	                           psPageArrayData->uiPid);
#endif
#endif

	if (psPageArrayData->bPoisonOnFree)
	{
		for (i = 0; i < uiNumPages; i++)
		{
			IMG_UINT32 idx = pai32FreeIndices ? pai32FreeIndices[i] : i;
			_PoisonDevicePage(psPageArrayData->psDevNode,
			                  ppsPageArray[idx],
			                  uiOrder,
			                  psPageArrayData->ui32CPUCacheFlags,
			                  PVRSRV_POISON_ON_FREE_VALUE);
		}
	}

	if (psPageArrayData->bIsCMA)
	{
		IMG_UINT32 uiDevNumPages = uiNumPages;
		IMG_UINT32 uiDevPageSize = 1<<psPageArrayData->uiLog2AllocPageSize;

		for (i = 0; i < uiDevNumPages; i++)
		{
			IMG_UINT32 idx = pai32FreeIndices ? pai32FreeIndices[i] : i;
			if (NULL != ppsPageArray[idx])
			{
				_FreeOSPage_CMA(psPageArrayData->psDevNode->psDevConfig->pvOSDevice,
								uiDevPageSize,
								uiOrder,
								psPageArrayData->dmavirtarray[idx],
								psPageArrayData->dmaphysarray[idx],
								ppsPageArray[idx]);
				psPageArrayData->dmaphysarray[idx] = (dma_addr_t)0;
				psPageArrayData->dmavirtarray[idx] = NULL;
				ppsPageArray[idx] = NULL;
				uiTempIdx++;
			}
		}
	}
	else
	{

		/* OSAllocMemNoStats required because this code may be run without the bridge lock held */
		ppsTempPageArray = OSAllocMemNoStats(sizeof(struct page*) * uiTempArraySize);
		if (ppsTempPageArray == NULL)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed free_pages metadata allocation", __func__));
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		/* Put pages in a contiguous array so further processing is easier */
		for (i = 0; i < uiNumPages; i++)
		{
			uiPageIndex = pai32FreeIndices ? pai32FreeIndices[i] : i;
			if (NULL != ppsPageArray[uiPageIndex])
			{
				struct page *psPage = ppsPageArray[uiPageIndex];

				for (j = 0; j < (1<<uiOrder); j++)
				{
					ppsTempPageArray[uiTempIdx] = psPage;
					uiTempIdx++;
					psPage++;
				}

				ppsPageArray[uiPageIndex] = NULL;
			}
		}

		/* Try to move the temp page array to the pool */
		bSuccess = _PutPagesToPoolLocked(psPageArrayData->ui32CPUCacheFlags,
										 ppsTempPageArray,
										 psPageArrayData->bUnpinned,
										 uiOrder,
										 uiTempIdx);
		if (bSuccess)
		{
			goto exit_ok;
		}

		/* Free pages and reset page caching attributes on x86 */
#if defined(CONFIG_X86)
		if (uiTempIdx != 0 && psPageArrayData->bUnsetMemoryType == IMG_TRUE)
		{
			int iError;
			iError = set_pages_array_wb(ppsTempPageArray, uiTempIdx);

			if (iError)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to reset page attributes", __func__));
			}
		}
#endif

		/* Free the pages */
		for (i = 0; i < uiTempIdx; i++)
		{
			__free_pages(ppsTempPageArray[i], uiOrder);
		}

		/* Free the temp page array here if it did not move to the pool */
		OSFreeMemNoStats(ppsTempPageArray);
	}

exit_ok:
	/* Update metadata */
	psPageArrayData->iNumOSPagesAllocated -= (uiTempIdx << uiOrder);
	PVR_ASSERT(0 <= psPageArrayData->iNumOSPagesAllocated);
	return PVRSRV_OK;
}

/* Free all the pages in a page array */
static PVRSRV_ERROR
_FreeOSPages_Fast(PMR_OSPAGEARRAY_DATA *psPageArrayData)
{
	IMG_BOOL bSuccess;
	IMG_UINT32 i;
	IMG_UINT32 uiNumPages = psPageArrayData->uiTotalNumOSPages;

	IMG_UINT32 uiOrder = psPageArrayData->uiLog2AllocPageSize - PAGE_SHIFT;
	IMG_UINT32 uiDevNumPages = uiNumPages >> uiOrder;
	IMG_UINT32 uiDevPageSize = PAGE_SIZE << uiOrder;
	struct page **ppsPageArray = psPageArrayData->pagearray;

	/* We really should have something to free before we call this */
	PVR_ASSERT(psPageArrayData->iNumOSPagesAllocated != 0);

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	for (i = 0; i < uiDevNumPages; i++)
	{
		_RemoveMemAllocRecord_UmaPages(psPageArrayData, ppsPageArray[i]);
	}
#else
	_DecrMemAllocStat_UmaPages(uiNumPages * PAGE_SIZE,
	                           psPageArrayData->uiPid);
#endif
#endif

	if (psPageArrayData->bPoisonOnFree)
	{
		for (i = 0; i < uiDevNumPages; i++)
		{
			_PoisonDevicePage(psPageArrayData->psDevNode,
			                  ppsPageArray[i],
			                  uiOrder,
			                  psPageArrayData->ui32CPUCacheFlags,
			                  PVRSRV_POISON_ON_FREE_VALUE);
		}
	}

	/* Try to move the page array to the pool */
	bSuccess = _PutPagesToPoolLocked(psPageArrayData->ui32CPUCacheFlags,
									 ppsPageArray,
									 psPageArrayData->bUnpinned,
									 uiOrder,
									 uiNumPages);
	if (bSuccess)
	{
		psPageArrayData->pagearray = NULL;
		goto exit_ok;
	}

	if (psPageArrayData->bIsCMA)
	{
		for (i = 0; i < uiDevNumPages; i++)
		{
			_FreeOSPage_CMA(psPageArrayData->psDevNode->psDevConfig->pvOSDevice,
							uiDevPageSize,
							uiOrder,
							psPageArrayData->dmavirtarray[i],
							psPageArrayData->dmaphysarray[i],
							ppsPageArray[i]);
			psPageArrayData->dmaphysarray[i] = (dma_addr_t)0;
			psPageArrayData->dmavirtarray[i] = NULL;
			ppsPageArray[i] = NULL;
		}
	}
	else
	{
#if defined(CONFIG_X86)
		if (psPageArrayData->bUnsetMemoryType == IMG_TRUE)
		{
			int ret;

			ret = set_pages_array_wb(ppsPageArray, uiNumPages);
			if (ret)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to reset page attributes", __FUNCTION__));
			}
		}
#endif

		for (i = 0; i < uiNumPages; i++)
		{
			_FreeOSPage(uiOrder, IMG_FALSE, ppsPageArray[i]);
			ppsPageArray[i] = NULL;
		}
	}

exit_ok:
	/* Update metadata */
	psPageArrayData->iNumOSPagesAllocated = 0;
	return PVRSRV_OK;
}

/* Free pages from a page array.
 * Takes care of mem stats and chooses correct free path depending on parameters. */
static PVRSRV_ERROR
_FreeOSPages(PMR_OSPAGEARRAY_DATA *psPageArrayData,
			 IMG_UINT32 *pai32FreeIndices,
			 IMG_UINT32 ui32FreePageCount)
{
	PVRSRV_ERROR eError;

	/* Go the sparse or non-sparse path */
	if (psPageArrayData->iNumOSPagesAllocated != psPageArrayData->uiTotalNumOSPages
		|| pai32FreeIndices != NULL)
	{
		eError = _FreeOSPages_Sparse(psPageArrayData,
									 pai32FreeIndices,
									 ui32FreePageCount);
	}
	else
	{
		eError = _FreeOSPages_Fast(psPageArrayData);
	}

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "_FreeOSPages_FreePages failed"));
	}

	_DumpPageArray(psPageArrayData->pagearray,
	               psPageArrayData->uiTotalNumOSPages >>
					   (psPageArrayData->uiLog2AllocPageSize - PAGE_SHIFT) );

	return eError;
}

/*
 *
 * Implementation of callback functions
 *
 */

/* Destructor func is called after last reference disappears, but
 * before PMR itself is freed. */
static PVRSRV_ERROR
PMRFinalizeOSMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PVRSRV_ERROR eError;
	PMR_OSPAGEARRAY_DATA *psOSPageArrayData = pvPriv;


	/* We can't free pages until now. */
	if (psOSPageArrayData->iNumOSPagesAllocated != 0)
	{
		_PagePoolLock();
		if (psOSPageArrayData->bUnpinned == IMG_TRUE)
		{
			_RemoveUnpinListEntryUnlocked(psOSPageArrayData);
		}
		_PagePoolUnlock();

		eError = _FreeOSPages(psOSPageArrayData,
							  NULL,
							  0);
		PVR_ASSERT (eError == PVRSRV_OK); /* can we do better? */
	}

	eError = _FreeOSPagesArray(psOSPageArrayData);
	PVR_ASSERT (eError == PVRSRV_OK); /* can we do better? */
	return PVRSRV_OK;
}

/* Callback function for locking the system physical page addresses.
 * This function must be called before the lookup address func. */
static PVRSRV_ERROR
PMRLockSysPhysAddressesOSMem(PMR_IMPL_PRIVDATA pvPriv)
{
	PVRSRV_ERROR eError;
	PMR_OSPAGEARRAY_DATA *psOSPageArrayData = pvPriv;

	if (psOSPageArrayData->bOnDemand)
	{
		/* Allocate Memory for deferred allocation */
		eError = _AllocOSPages(psOSPageArrayData, NULL, psOSPageArrayData->uiTotalNumOSPages);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}
	}

	eError = PVRSRV_OK;
	return eError;
}

static PVRSRV_ERROR
PMRUnlockSysPhysAddressesOSMem(PMR_IMPL_PRIVDATA pvPriv)
{
	/* Just drops the refcount. */
	PVRSRV_ERROR eError = PVRSRV_OK;
	PMR_OSPAGEARRAY_DATA *psOSPageArrayData = pvPriv;

	if (psOSPageArrayData->bOnDemand)
	{
		/* Free Memory for deferred allocation */
		eError = _FreeOSPages(psOSPageArrayData,
							  NULL,
							  0);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}
	}

	PVR_ASSERT (eError == PVRSRV_OK);
	return eError;
}

/* N.B. It is assumed that PMRLockSysPhysAddressesOSMem() is called _before_ this function! */
static PVRSRV_ERROR
PMRSysPhysAddrOSMem(PMR_IMPL_PRIVDATA pvPriv,
					IMG_UINT32 ui32Log2PageSize,
					IMG_UINT32 ui32NumOfPages,
					IMG_DEVMEM_OFFSET_T *puiOffset,
					IMG_BOOL *pbValid,
					IMG_DEV_PHYADDR *psDevPAddr)
{
	const PMR_OSPAGEARRAY_DATA *psOSPageArrayData = pvPriv;
	IMG_UINT32 uiPageSize = 1U << psOSPageArrayData->uiLog2AllocPageSize;
	IMG_UINT32 uiInPageOffset;
	IMG_UINT32 uiPageIndex;
	IMG_UINT32 uiIdx;

	if (psOSPageArrayData->uiLog2AllocPageSize < ui32Log2PageSize)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Requested physical addresses from PMR "
		         "for incompatible contiguity %u!",
		         __FUNCTION__,
		         ui32Log2PageSize));
		return PVRSRV_ERROR_PMR_INCOMPATIBLE_CONTIGUITY;
	}

	for (uiIdx=0; uiIdx < ui32NumOfPages; uiIdx++)
	{
		if (pbValid[uiIdx])
		{
			uiPageIndex = puiOffset[uiIdx] >> psOSPageArrayData->uiLog2AllocPageSize;
			uiInPageOffset = puiOffset[uiIdx] - ((IMG_DEVMEM_OFFSET_T)uiPageIndex << psOSPageArrayData->uiLog2AllocPageSize);

			PVR_ASSERT(uiPageIndex < psOSPageArrayData->uiTotalNumOSPages);
			PVR_ASSERT(uiInPageOffset < uiPageSize);

#if defined(CONFIG_L4)
			psDevPAddr[uiIdx].uiAddr = l4x_virt_to_phys((void *)((phys_addr_t)page_to_pfn(psOSPageArrayData->pagearray[uiPageIndex]) << PAGE_SHIFT));
#else
			psDevPAddr[uiIdx].uiAddr = page_to_phys(psOSPageArrayData->pagearray[uiPageIndex]);
#endif
			psDevPAddr[uiIdx].uiAddr += uiInPageOffset;

#if !defined(PVR_LINUX_PHYSMEM_USE_HIGHMEM_ONLY)
			/* this is just a precaution, normally this should be always
			 * available */
			if (psOSPageArrayData->ui64DmaMask)
			{
				if (psDevPAddr[uiIdx].uiAddr > psOSPageArrayData->ui64DmaMask)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s: physical address"
							" (%" IMG_UINT64_FMTSPECX ") out of allowable range"
							" [0; %" IMG_UINT64_FMTSPECX "]", __func__,
							psDevPAddr[uiIdx].uiAddr,
							psOSPageArrayData->ui64DmaMask));
					BUG();
				}
			}
#endif
		}
	}

	return PVRSRV_OK;
}

typedef struct _PMR_OSPAGEARRAY_KERNMAP_DATA_ {
	void *pvBase;
	IMG_UINT32 ui32PageCount;
} PMR_OSPAGEARRAY_KERNMAP_DATA;

static PVRSRV_ERROR
PMRAcquireKernelMappingDataOSMem(PMR_IMPL_PRIVDATA pvPriv,
								 size_t uiOffset,
								 size_t uiSize,
								 void **ppvKernelAddressOut,
								 IMG_HANDLE *phHandleOut,
								 PMR_FLAGS_T ulFlags)
{
	PVRSRV_ERROR eError;
	PMR_OSPAGEARRAY_DATA *psOSPageArrayData = pvPriv;
	void *pvAddress;
	pgprot_t prot = PAGE_KERNEL;
	IMG_UINT32 ui32PageOffset=0;
	size_t uiMapOffset=0;
	IMG_UINT32 ui32PageCount=0;
	IMG_UINT32 uiLog2AllocPageSize = psOSPageArrayData->uiLog2AllocPageSize;
	IMG_UINT32 uiOSPageShift = OSGetPageShift();
	IMG_UINT32 uiMapPageCount = 0;
	IMG_INT32 uiPageSizeDiff = 0;
	struct page **pagearray;
	PMR_OSPAGEARRAY_KERNMAP_DATA *psData;

	/* For cases device page size greater than the OS page size,
	 * multiple physically contiguous OS pages constitute one device page.
	 * However only the first page address of such an ensemble is stored
	 * as part of the mapping table. Hence when mapping the PMR in part /full,
	 * all pages that constitute the device page must also be mapped to kernel.
	 *
	 * For the case where device page size less than OS page size,
	 * treat it the same way as the page sizes are equal */
	if (uiLog2AllocPageSize > uiOSPageShift)
	{
		uiPageSizeDiff = uiLog2AllocPageSize - uiOSPageShift;
	}

	/*
		Zero offset and size as a special meaning which means map in the
		whole of the PMR, this is due to fact that the places that call
		this callback might not have access to be able to determine the
		physical size
	*/
	if ((uiOffset == 0) && (uiSize == 0))
	{
		ui32PageOffset = 0;
		uiMapOffset = 0;
		ui32PageCount = psOSPageArrayData->iNumOSPagesAllocated;
	}
	else
	{
		size_t uiEndoffset;

		ui32PageOffset = uiOffset >> uiLog2AllocPageSize;
		uiMapOffset = uiOffset - (ui32PageOffset << uiLog2AllocPageSize);
		uiEndoffset = uiOffset + uiSize - 1;
		/* Add one as we want the count, not the offset */
		ui32PageCount = (uiEndoffset >> uiLog2AllocPageSize) + 1;
		ui32PageCount -= ui32PageOffset;
	}

	/* The page count to be mapped might be different if the
	 * OS page size is lesser than the device page size */
	uiMapPageCount = ui32PageCount << uiPageSizeDiff;

	switch (PVRSRV_CPU_CACHE_MODE(psOSPageArrayData->ui32CPUCacheFlags))
	{
		case PVRSRV_MEMALLOCFLAG_CPU_UNCACHED:
				prot = pgprot_noncached(prot);
				break;

		case PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE:
				prot = pgprot_writecombine(prot);
				break;

		case PVRSRV_MEMALLOCFLAG_CPU_CACHED:
				break;

		default:
				eError = PVRSRV_ERROR_INVALID_PARAMS;
				goto e0;
	}

	if (uiPageSizeDiff)
	{
		/* Map all the individual OS page that is part of a single device page. */
		IMG_UINT32 ui32Temp = 0;
		struct page **psPage = &psOSPageArrayData->pagearray[ui32PageOffset];

		/* Allocate enough memory for the page pointers for this mapping */
		pagearray = OSAllocMem(uiMapPageCount * sizeof(pagearray[0]));

		if (pagearray == NULL)
		{
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto e0;
		}

		/* construct array that holds the page pointers that constitute the requested
		 * mapping */
		while(ui32Temp < ui32PageCount)
		{
			IMG_UINT32 ui32SubPage = 0, ui32SubPageCount = (1 << uiPageSizeDiff);

			/* Fill in the page pointers for the sub pages that constitute a single
			 * device page */
			for(;ui32SubPage < ui32SubPageCount; ui32SubPage++)
			{
				pagearray[ui32Temp + ui32SubPage] = psPage[ui32Temp]+ui32SubPage;
			}
			ui32Temp++;
		}
	}
	else
	{
		pagearray = &psOSPageArrayData->pagearray[ui32PageOffset];
	}

	psData = OSAllocMem(sizeof(*psData));
	if (psData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e1;
	}

#if !defined(CONFIG_64BIT) || defined(PVRSRV_FORCE_SLOWER_VMAP_ON_64BIT_BUILDS)
	pvAddress = vmap(pagearray, uiMapPageCount, VM_READ | VM_WRITE, prot);
#else
	pvAddress = vm_map_ram(pagearray, uiMapPageCount, -1, prot);
#endif
	if (pvAddress == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e2;
	}

	*ppvKernelAddressOut = pvAddress + uiMapOffset;
	psData->pvBase = pvAddress;
	psData->ui32PageCount = uiMapPageCount;
	*phHandleOut = psData;

	if (uiPageSizeDiff)
	{
		OSFreeMem(pagearray);
	}

	return PVRSRV_OK;

	/*
	  error exit paths follow
	*/
 e2:
	OSFreeMem(psData);
 e1:
	if (uiPageSizeDiff)
	{
		OSFreeMem(pagearray);
	}
 e0:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

static void PMRReleaseKernelMappingDataOSMem(PMR_IMPL_PRIVDATA pvPriv,
											 IMG_HANDLE hHandle)
{
	PMR_OSPAGEARRAY_KERNMAP_DATA *psData = hHandle;
	PVR_UNREFERENCED_PARAMETER(pvPriv);

#if !defined(CONFIG_64BIT) || defined(PVRSRV_FORCE_SLOWER_VMAP_ON_64BIT_BUILDS)
	vunmap(psData->pvBase);
#else
	vm_unmap_ram(psData->pvBase, psData->ui32PageCount);
#endif
	OSFreeMem(psData);
}

static
PVRSRV_ERROR PMRUnpinOSMem(PMR_IMPL_PRIVDATA pPriv)
{
	PMR_OSPAGEARRAY_DATA *psOSPageArrayData = pPriv;
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Lock down the pool and add the array to the unpin list */
	_PagePoolLock();

	/* Sanity check */
	PVR_ASSERT(psOSPageArrayData->bUnpinned == IMG_FALSE);
	PVR_ASSERT(psOSPageArrayData->bOnDemand == IMG_FALSE);

	eError = _AddUnpinListEntryUnlocked(psOSPageArrayData);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Not able to add allocation to unpinned list (%d).",
		         __FUNCTION__,
		         eError));

		goto e_exit;
	}

	psOSPageArrayData->bUnpinned = IMG_TRUE;

e_exit:
	_PagePoolUnlock();
	return eError;
}

static
PVRSRV_ERROR PMRPinOSMem(PMR_IMPL_PRIVDATA pPriv,
						PMR_MAPPING_TABLE *psMappingTable)
{
	PVRSRV_ERROR eError;
	PMR_OSPAGEARRAY_DATA *psOSPageArrayData = pPriv;
	IMG_UINT32  *pui32MapTable = NULL;
	IMG_UINT32 i,j=0, ui32Temp=0;

	_PagePoolLock();

	/* Sanity check */
	PVR_ASSERT(psOSPageArrayData->bUnpinned == IMG_TRUE);

	psOSPageArrayData->bUnpinned = IMG_FALSE;

	/* If there are still pages in the array remove entries from the pool */
	if (psOSPageArrayData->iNumOSPagesAllocated != 0)
	{
		_RemoveUnpinListEntryUnlocked(psOSPageArrayData);
		_PagePoolUnlock();

		eError = PVRSRV_OK;
		goto e_exit_mapalloc_failure;
	}
	_PagePoolUnlock();

	/* If pages were reclaimed we allocate new ones and
	 * return PVRSRV_ERROR_PMR_NEW_MEMORY */
	if (psMappingTable->ui32NumVirtChunks == 1)
	{
		eError = _AllocOSPages(psOSPageArrayData, NULL, psOSPageArrayData->uiTotalNumOSPages);
	}
	else
	{
		pui32MapTable = (IMG_UINT32 *)OSAllocMem(sizeof(*pui32MapTable) * psMappingTable->ui32NumPhysChunks);
		if (NULL == pui32MapTable)
		{
			eError = PVRSRV_ERROR_PMR_FAILED_TO_ALLOC_PAGES;
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Not able to Alloc Map Table.",
					 __FUNCTION__));
			goto e_exit_mapalloc_failure;
		}

		for (i = 0,j=0; i < psMappingTable->ui32NumVirtChunks; i++)
		{
			ui32Temp = psMappingTable->aui32Translation[i];
			if (TRANSLATION_INVALID != ui32Temp)
			{
				pui32MapTable[j++] = ui32Temp;
			}
		}
		eError = _AllocOSPages(psOSPageArrayData, pui32MapTable, psMappingTable->ui32NumPhysChunks);
	}

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Not able to get new pages for unpinned allocation.",
				 __FUNCTION__));

		eError = PVRSRV_ERROR_PMR_FAILED_TO_ALLOC_PAGES;
		goto e_exit;
	}

	PVR_DPF((PVR_DBG_MESSAGE,
			 "%s: Allocating new pages for unpinned allocation. "
			 "Old content is lost!",
			 __FUNCTION__));

	eError = PVRSRV_ERROR_PMR_NEW_MEMORY;

e_exit:
	OSFreeMem(pui32MapTable);
e_exit_mapalloc_failure:
	return eError;
}

/*************************************************************************/ /*!
@Function       PMRChangeSparseMemOSMem
@Description    This function Changes the sparse mapping by allocating & freeing
				of pages. It does also change the GPU and CPU maps accordingly
@Return         PVRSRV_ERROR failure code
*/ /**************************************************************************/
static PVRSRV_ERROR
PMRChangeSparseMemOSMem(PMR_IMPL_PRIVDATA pPriv,
						const PMR *psPMR,
						IMG_UINT32 ui32AllocPageCount,
						IMG_UINT32 *pai32AllocIndices,
						IMG_UINT32 ui32FreePageCount,
						IMG_UINT32 *pai32FreeIndices,
						IMG_UINT32 uiFlags)
{
	PVRSRV_ERROR eError;

	PMR_MAPPING_TABLE *psPMRMapTable = PMR_GetMappigTable(psPMR);
	PMR_OSPAGEARRAY_DATA *psPMRPageArrayData = (PMR_OSPAGEARRAY_DATA *)pPriv;
	struct page **psPageArray = psPMRPageArrayData->pagearray;
	void **psDMAVirtArray = psPMRPageArrayData->dmavirtarray;
	dma_addr_t *psDMAPhysArray = psPMRPageArrayData->dmaphysarray;

	struct page *psPage;
	dma_addr_t psDMAPAddr;
	void *pvDMAVAddr;

	IMG_UINT32 ui32AdtnlAllocPages = 0; /*<! Number of pages to alloc from the OS */
	IMG_UINT32 ui32AdtnlFreePages = 0; /*<! Number of pages to free back to the OS */
	IMG_UINT32 ui32CommonRequestCount = 0; /*<! Number of pages to move position in the page array */
	IMG_UINT32 ui32Loop = 0;
	IMG_UINT32 ui32Index = 0;
	IMG_UINT32 uiAllocpgidx;
	IMG_UINT32 uiFreepgidx;
	IMG_UINT32 uiOrder = psPMRPageArrayData->uiLog2AllocPageSize - PAGE_SHIFT;
	IMG_BOOL bCMA = psPMRPageArrayData->bIsCMA;


	/* Check SPARSE flags and calculate pages to allocate and free */
	if (SPARSE_RESIZE_BOTH == (uiFlags & SPARSE_RESIZE_BOTH))
	{
		ui32CommonRequestCount = (ui32AllocPageCount > ui32FreePageCount) ?
				ui32FreePageCount : ui32AllocPageCount;

		PDUMP_PANIC(SPARSEMEM_SWAP, "Request to swap alloc & free pages not supported");
	}

	if (SPARSE_RESIZE_ALLOC == (uiFlags & SPARSE_RESIZE_ALLOC))
	{
		ui32AdtnlAllocPages = ui32AllocPageCount - ui32CommonRequestCount;
	}
	else
	{
		ui32AllocPageCount = 0;
	}

	if (SPARSE_RESIZE_FREE == (uiFlags & SPARSE_RESIZE_FREE))
	{
		ui32AdtnlFreePages = ui32FreePageCount - ui32CommonRequestCount;
	}
	else
	{
		ui32FreePageCount = 0;
	}

	if (0 == (ui32CommonRequestCount || ui32AdtnlAllocPages || ui32AdtnlFreePages))
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Missing parameters for number of pages to alloc/free",
		         __FUNCTION__));
		return eError;
	}

	/* The incoming request is classified into two operations independent of
	 * each other: alloc & free pages.
	 * These operations can be combined with two mapping operations as well
	 * which are GPU & CPU space mappings.
	 *
	 * From the alloc and free page requests, the net amount of pages to be
	 * allocated or freed is computed. Pages that were requested to be freed
	 * will be reused to fulfil alloc requests.
	 *
	 * The order of operations is:
	 * 1. Allocate new pages from the OS
	 * 2. Move the free pages from free request to alloc positions.
	 * 3. Free the rest of the pages not used for alloc
	 *
	 * Alloc parameters are validated at the time of allocation
	 * and any error will be handled then. */

	/* Validate the free indices */
	if (ui32FreePageCount)
	{
		if (NULL != pai32FreeIndices){

			for (ui32Loop = 0; ui32Loop < ui32FreePageCount; ui32Loop++)
			{
				uiFreepgidx = pai32FreeIndices[ui32Loop];

				if (uiFreepgidx > (psPMRPageArrayData->uiTotalNumOSPages >> uiOrder))
				{
					eError = PVRSRV_ERROR_DEVICEMEM_OUT_OF_RANGE;
					goto e0;
				}

				if (NULL == psPageArray[uiFreepgidx])
				{
					eError = PVRSRV_ERROR_INVALID_PARAMS;
					PVR_DPF((PVR_DBG_ERROR,
					         "%s: Trying to free non-allocated page",
					         __FUNCTION__));
					goto e0;
				}
			}
		}
		else
		{
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Given non-zero free count but missing indices array",
			         __FUNCTION__));
			return eError;
		}
	}

	/* Validate the alloc indices */
	for (ui32Loop = ui32AdtnlAllocPages; ui32Loop < ui32AllocPageCount; ui32Loop++)
	{
		uiAllocpgidx = pai32AllocIndices[ui32Loop];

		if (uiAllocpgidx > (psPMRPageArrayData->uiTotalNumOSPages >> uiOrder))
		{
			eError = PVRSRV_ERROR_DEVICEMEM_OUT_OF_RANGE;
			goto e0;
		}

		if (SPARSE_REMAP_MEM != (uiFlags & SPARSE_REMAP_MEM))
		{
			if ((NULL != psPageArray[uiAllocpgidx]) ||
			    (TRANSLATION_INVALID != psPMRMapTable->aui32Translation[uiAllocpgidx]))
			{
				eError = PVRSRV_ERROR_INVALID_PARAMS;
				PVR_DPF((PVR_DBG_ERROR,
				         "%s: Trying to allocate already allocated page again",
				         __FUNCTION__));
				goto e0;
			}
		}
		else
		{
			if ((NULL == psPageArray[uiAllocpgidx]) ||
			    (TRANSLATION_INVALID == psPMRMapTable->aui32Translation[uiAllocpgidx]) )
			{
				eError = PVRSRV_ERROR_INVALID_PARAMS;
				PVR_DPF((PVR_DBG_ERROR,
				         "%s: Unable to remap memory due to missing page",
				         __FUNCTION__));
				goto e0;
			}
		}
	}

	ui32Loop = 0;

	/* Allocate new pages from the OS */
	if (0 != ui32AdtnlAllocPages)
	{
			eError = _AllocOSPages(psPMRPageArrayData, pai32AllocIndices, ui32AdtnlAllocPages);
			if (PVRSRV_OK != eError)
			{
				PVR_DPF((PVR_DBG_MESSAGE,
				         "%s: New Addtl Allocation of pages failed",
				         __FUNCTION__));
				goto e0;
			}

			psPMRMapTable->ui32NumPhysChunks += ui32AdtnlAllocPages;
			/*Mark the corresponding pages of translation table as valid */
			for (ui32Loop = 0; ui32Loop < ui32AdtnlAllocPages; ui32Loop++)
			{
				psPMRMapTable->aui32Translation[pai32AllocIndices[ui32Loop]] = pai32AllocIndices[ui32Loop];
			}
	}


	ui32Index = ui32Loop;

	/* Move the corresponding free pages to alloc request */
	for (ui32Loop = 0; ui32Loop < ui32CommonRequestCount; ui32Loop++, ui32Index++)
	{
		uiAllocpgidx = pai32AllocIndices[ui32Index];
		uiFreepgidx =  pai32FreeIndices[ui32Loop];

		psPage = psPageArray[uiAllocpgidx];
		psPageArray[uiAllocpgidx] = psPageArray[uiFreepgidx];

		if (bCMA)
		{
			pvDMAVAddr = psDMAVirtArray[uiAllocpgidx];
			psDMAPAddr = psDMAPhysArray[uiAllocpgidx];
			psDMAVirtArray[uiAllocpgidx] = psDMAVirtArray[uiFreepgidx];
			psDMAPhysArray[uiAllocpgidx] = psDMAPhysArray[uiFreepgidx];
		}

		/* Is remap mem used in real world scenario? Should it be turned to a
		 *  debug feature? The condition check needs to be out of loop, will be
		 *  done at later point though after some analysis */
		if (SPARSE_REMAP_MEM != (uiFlags & SPARSE_REMAP_MEM))
		{
			psPMRMapTable->aui32Translation[uiFreepgidx] = TRANSLATION_INVALID;
			psPMRMapTable->aui32Translation[uiAllocpgidx] = uiAllocpgidx;
			psPageArray[uiFreepgidx] = NULL;
			if (bCMA)
			{
				psDMAVirtArray[uiFreepgidx] = NULL;
				psDMAPhysArray[uiFreepgidx] = (dma_addr_t)0;
			}
		}
		else
		{
			psPMRMapTable->aui32Translation[uiFreepgidx] = uiFreepgidx;
			psPMRMapTable->aui32Translation[uiAllocpgidx] = uiAllocpgidx;
			psPageArray[uiFreepgidx] = psPage;
			if (bCMA)
			{
				psDMAVirtArray[uiFreepgidx] = pvDMAVAddr;
				psDMAPhysArray[uiFreepgidx] = psDMAPAddr;
			}
		}
	}

	/* Free the additional free pages */
	if (0 != ui32AdtnlFreePages)
	{
		eError = _FreeOSPages(psPMRPageArrayData,
		                      &pai32FreeIndices[ui32Loop],
		                      ui32AdtnlFreePages);
		if (eError != PVRSRV_OK)
		{
			goto e0;
		}
		psPMRMapTable->ui32NumPhysChunks -= ui32AdtnlFreePages;
		while (ui32Loop < ui32FreePageCount)
		{
			psPMRMapTable->aui32Translation[pai32FreeIndices[ui32Loop]] = TRANSLATION_INVALID;
			ui32Loop++;
		}
	}

	eError = PVRSRV_OK;

e0:
	return eError;
}

/*************************************************************************/ /*!
@Function       PMRChangeSparseMemCPUMapOSMem
@Description    This function Changes CPU maps accordingly
@Return         PVRSRV_ERROR failure code
*/ /**************************************************************************/
static
PVRSRV_ERROR PMRChangeSparseMemCPUMapOSMem(PMR_IMPL_PRIVDATA pPriv,
                                           const PMR *psPMR,
                                           IMG_UINT64 sCpuVAddrBase,
                                           IMG_UINT32 ui32AllocPageCount,
                                           IMG_UINT32 *pai32AllocIndices,
                                           IMG_UINT32 ui32FreePageCount,
                                           IMG_UINT32 *pai32FreeIndices)
{
	struct page **psPageArray;
	PMR_OSPAGEARRAY_DATA *psPMRPageArrayData = (PMR_OSPAGEARRAY_DATA *)pPriv;
	IMG_CPU_PHYADDR sCPUPAddr;

	sCPUPAddr.uiAddr = 0;
	psPageArray = psPMRPageArrayData->pagearray;

	return OSChangeSparseMemCPUAddrMap((void **)psPageArray,
	                                   sCpuVAddrBase,
	                                   sCPUPAddr,
	                                   ui32AllocPageCount,
	                                   pai32AllocIndices,
	                                   ui32FreePageCount,
	                                   pai32FreeIndices,
	                                   IMG_FALSE);
}

static PMR_IMPL_FUNCTAB _sPMROSPFuncTab = {
	.pfnLockPhysAddresses = &PMRLockSysPhysAddressesOSMem,
	.pfnUnlockPhysAddresses = &PMRUnlockSysPhysAddressesOSMem,
	.pfnDevPhysAddr = &PMRSysPhysAddrOSMem,
	.pfnAcquireKernelMappingData = &PMRAcquireKernelMappingDataOSMem,
	.pfnReleaseKernelMappingData = &PMRReleaseKernelMappingDataOSMem,
	.pfnReadBytes = NULL,
	.pfnWriteBytes = NULL,
	.pfnUnpinMem = &PMRUnpinOSMem,
	.pfnPinMem = &PMRPinOSMem,
	.pfnChangeSparseMem = &PMRChangeSparseMemOSMem,
	.pfnChangeSparseMemCPUMap = &PMRChangeSparseMemCPUMapOSMem,
	.pfnFinalize = &PMRFinalizeOSMem,
};

PVRSRV_ERROR
PhysmemNewOSRamBackedPMR(PVRSRV_DEVICE_NODE *psDevNode,
						 IMG_DEVMEM_SIZE_T uiSize,
						 IMG_DEVMEM_SIZE_T uiChunkSize,
						 IMG_UINT32 ui32NumPhysChunks,
						 IMG_UINT32 ui32NumVirtChunks,
						 IMG_UINT32 *puiAllocIndices,
						 IMG_UINT32 uiLog2AllocPageSize,
						 PVRSRV_MEMALLOCFLAGS_T uiFlags,
						 const IMG_CHAR *pszAnnotation,
						 IMG_PID uiPid,
						 PMR **ppsPMRPtr)
{
	PVRSRV_ERROR eError;
	PVRSRV_ERROR eError2;
	PMR *psPMR;
	struct _PMR_OSPAGEARRAY_DATA_ *psPrivData;
	PMR_FLAGS_T uiPMRFlags;
	PHYS_HEAP *psPhysHeap;
	IMG_UINT32 ui32CPUCacheFlags;
	IMG_BOOL bZero;
	IMG_BOOL bIsCMA;
	IMG_BOOL bPoisonOnAlloc;
	IMG_BOOL bPoisonOnFree;
	IMG_BOOL bOnDemand;
	IMG_BOOL bCpuLocal;
	IMG_BOOL bFwLocal;

	/*
	 * The host driver (but not guest) can still use this factory for firmware
	 * allocations
	 */
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) && PVRSRV_CHECK_FW_LOCAL(uiFlags))
	{
		PVR_ASSERT(0);
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto errorOnParam;
	}

	/* Select correct caching mode */
	eError = DevmemCPUCacheMode(psDevNode, uiFlags, &ui32CPUCacheFlags);
	if (eError != PVRSRV_OK)
	{
		goto errorOnParam;
	}

	if (PVRSRV_CHECK_CPU_CACHE_CLEAN(uiFlags))
	{
		ui32CPUCacheFlags |= PVRSRV_MEMALLOCFLAG_CPU_CACHE_CLEAN;
	}

	/*
	 * Use CMA framework if order is greater than OS page size; please note
	 * that OSMMapPMRGeneric() has the same expectation as well.
	 */
	bIsCMA = uiLog2AllocPageSize > PAGE_SHIFT ? IMG_TRUE : IMG_FALSE;
	bOnDemand = PVRSRV_CHECK_ON_DEMAND(uiFlags) ? IMG_TRUE : IMG_FALSE;
	bCpuLocal = PVRSRV_CHECK_CPU_LOCAL(uiFlags) ? IMG_TRUE : IMG_FALSE;
	bFwLocal = PVRSRV_CHECK_FW_LOCAL(uiFlags) ? IMG_TRUE : IMG_FALSE;
	bZero = PVRSRV_CHECK_ZERO_ON_ALLOC(uiFlags) ? IMG_TRUE : IMG_FALSE;
	bPoisonOnAlloc = PVRSRV_CHECK_POISON_ON_ALLOC(uiFlags) ? IMG_TRUE : IMG_FALSE;
	bPoisonOnFree = PVRSRV_CHECK_POISON_ON_FREE(uiFlags) ? IMG_TRUE : IMG_FALSE;

#if defined(PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES)
	/* Overwrite flags and always zero pages that could go back to UM */
	bZero = IMG_TRUE;
	bPoisonOnAlloc = IMG_FALSE;
#endif
#if defined(CONFIG_L4)
	/* Overwrite, use CMA for all allocations if running in native mode */
	bIsCMA = PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE) ? IMG_TRUE : bIsCMA;
#endif

	/* Physical allocation alignment is generally not supported except under
	   very restrictive conditions, also there is a maximum alignment value
	   which must not exceed the largest device page-size. If these are not
	   met then fail the aligned-requested allocation */
	if (bIsCMA)
	{
		IMG_UINT32 uiAlign = 1 << uiLog2AllocPageSize;
		if (uiAlign > uiSize || uiAlign > (1 << PVR_MAX_PHYSMEM_CONTIG_ALLOC_LOG2PGSZ))
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Invalid PA alignment: size 0x%llx, align 0x%x",
					__FUNCTION__, uiSize, uiAlign));
			eError = PVRSRV_ERROR_INVALID_ALIGNMENT;
			goto errorOnParam;
		}
#if !defined(CONFIG_L4)
		PVR_ASSERT(uiLog2AllocPageSize > PVR_MIN_PHYSMEM_CONTIG_ALLOC_LOG2PGSZ);
#endif
	}

	/* Create Array structure that hold the physical pages */
	eError = _AllocOSPageArray(psDevNode,
							   uiChunkSize,
							   ui32NumPhysChunks,
							   ui32NumVirtChunks,
							   uiLog2AllocPageSize,
							   bZero,
							   bIsCMA,
							   bPoisonOnAlloc,
							   bPoisonOnFree,
							   bOnDemand,
							   ui32CPUCacheFlags,
							   uiPid,
							   &psPrivData);
	if (eError != PVRSRV_OK)
	{
		goto errorOnAllocPageArray;
	}

	if (!bOnDemand)
	{
		/* Do we fill the whole page array or just parts (sparse)? */
		if (ui32NumPhysChunks == ui32NumVirtChunks)
		{
			/* Allocate the physical pages */
			eError = _AllocOSPages(psPrivData,
			                       NULL,
			                       psPrivData->uiTotalNumOSPages >>
									   (uiLog2AllocPageSize - PAGE_SHIFT) );
		}
		else
		{
			if (ui32NumPhysChunks != 0)
			{
				/* Calculate the number of pages we want to allocate */
				IMG_UINT32 uiPagesToAlloc =
					(IMG_UINT32) ((((ui32NumPhysChunks * uiChunkSize) - 1) >> uiLog2AllocPageSize) + 1);

				/* Make sure calculation is correct */
				PVR_ASSERT(((PMR_SIZE_T) uiPagesToAlloc << uiLog2AllocPageSize) ==
						   (ui32NumPhysChunks * uiChunkSize) );

				/* Allocate the physical pages */
				eError = _AllocOSPages(psPrivData, puiAllocIndices,
									   uiPagesToAlloc);
			}
		}

		if (eError != PVRSRV_OK)
		{
			goto errorOnAllocPages;
		}
	}

	/*
	 * In this instance, we simply pass flags straight through.
	 *
	 * Generically, uiFlags can include things that control the PMR factory, but
	 * we don't need any such thing (at the time of writing!), and our caller
	 * specifies all PMR flags so we don't need to meddle with what was given to
	 * us.
	 */
	uiPMRFlags = (PMR_FLAGS_T)(uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK);

	/*
	 * Check no significant bits were lost in cast due to different bit widths
	 * for flags
	 */
	PVR_ASSERT(uiPMRFlags == (uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK));

	if (bOnDemand)
	{
		PDUMPCOMMENT("Deferred Allocation PMR (UMA)");
	}

	if (bFwLocal)
	{
		PDUMPCOMMENT("FW_LOCAL allocation requested");
		psPhysHeap = psDevNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL];
	}
	else if (bCpuLocal)
	{
		PDUMPCOMMENT("CPU_LOCAL allocation requested");
		psPhysHeap = psDevNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL];
	}
	else
	{
		psPhysHeap = psDevNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL];
	}

	eError = PMRCreatePMR(psDevNode,
						  psPhysHeap,
						  uiSize,
						  uiChunkSize,
						  ui32NumPhysChunks,
						  ui32NumVirtChunks,
						  puiAllocIndices,
						  uiLog2AllocPageSize,
						  uiPMRFlags,
						  pszAnnotation,
						  &_sPMROSPFuncTab,
						  psPrivData,
						  PMR_TYPE_OSMEM,
						  &psPMR,
						  PDUMP_NONE);
	if (eError != PVRSRV_OK)
	{
		goto errorOnCreate;
	}

	*ppsPMRPtr = psPMR;

	return PVRSRV_OK;

errorOnCreate:
	if (!bOnDemand)
	{
		eError2 = _FreeOSPages(psPrivData, NULL, 0);
		PVR_ASSERT(eError2 == PVRSRV_OK);
	}

errorOnAllocPages:
	eError2 = _FreeOSPagesArray(psPrivData);
	PVR_ASSERT(eError2 == PVRSRV_OK);

errorOnAllocPageArray:
errorOnParam:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}
