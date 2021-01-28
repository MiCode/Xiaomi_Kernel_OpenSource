/*************************************************************************/ /*!
@File
@Title          Host memory management implementation for Linux
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

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/string.h>

#include "img_defs.h"
#include "allocmem.h"
#include "pvr_debug.h"
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif
#include "osfunc.h"

/*
 * PVRSRV_ENABLE_PROCESS_STATS enables process statistics regarding events,
 *     resources and memory across all processes
 * PVRSRV_ENABLE_MEMORY_STATS enables recording of Linux kernel memory
 *     allocations, provided that PVRSRV_ENABLE_PROCESS_STATS is enabled
 *   - Output can be found in:
 *     /sys/kernel/debug/pvr/proc_stats/[live|retired]_pids_stats/mem_area
 * PVRSRV_DEBUG_LINUX_MEMORY_STATS provides more details about memory
 *     statistics in conjunction with PVRSRV_ENABLE_MEMORY_STATS
 * PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON is defined to encompass both memory
 *     allocation statistics functionalities described above in a single macro
 */
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && defined(PVRSRV_ENABLE_MEMORY_STATS) && defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS) && defined(DEBUG)
#define PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON
#endif

/*
 * When using detailed memory allocation statistics, the line number and
 * file name where the allocation happened are also provided.
 * When this feature is not used, these parameters are not needed.
 */
#if defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON)
#define DEBUG_MEMSTATS_PARAMS ,void *pvAllocFromFile, IMG_UINT32 ui32AllocFromLine
#define DEBUG_MEMSTATS_VALUES ,pvAllocFromFile, ui32AllocFromLine
#else
#define DEBUG_MEMSTATS_PARAMS
#define DEBUG_MEMSTATS_VALUES
#endif

/*
 * When memory statistics are disabled, memory records are used instead.
 * In order for these to work, the PID of the process that requested the
 * allocation needs to be stored at the end of the kmalloc'd memory, making
 * sure 4 extra bytes are allocated to fit the PID.
 *
 * There is no need for this extra allocation when memory statistics are
 * enabled, since all allocations are tracked in DebugFS mem_area files.
 */
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_ENABLE_MEMORY_STATS)
#define ALLOCMEM_MEMSTATS_PADDING sizeof(IMG_UINT32)
#else
#define ALLOCMEM_MEMSTATS_PADDING 0UL
#endif

static inline void _pvr_vfree(const void* pvAddr)
{
#if defined(DEBUG)
	/* Size harder to come by for vmalloc and since vmalloc allocates
	 * a whole number of pages, poison the minimum size known to have
	 * been allocated.
	 */
	OSCachedMemSet((void*)pvAddr, PVRSRV_POISON_ON_ALLOC_VALUE,
	               PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD);
#endif
	vfree(pvAddr);
}

static inline void _pvr_kfree(const void* pvAddr)
{
#if defined(DEBUG)
	/* Poison whole memory block */
	OSCachedMemSet((void*)pvAddr, PVRSRV_POISON_ON_ALLOC_VALUE,
	               ksize(pvAddr));
#endif
	kfree(pvAddr);
}

static inline void _pvr_alloc_stats_add(void *pvAddr, IMG_UINT32 ui32Size DEBUG_MEMSTATS_PARAMS)
{
#if !defined(PVRSRV_ENABLE_PROCESS_STATS)
	PVR_UNREFERENCED_PARAMETER(pvAddr);
#else
	if (!is_vmalloc_addr(pvAddr))
	{
#if defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON)
		IMG_CPU_PHYADDR sCpuPAddr;
		sCpuPAddr.uiAddr = 0;

		_PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_KMALLOC,
									  pvAddr,
									  sCpuPAddr,
									  ksize(pvAddr),
									  NULL,
									  OSGetCurrentClientProcessIDKM()
									  DEBUG_MEMSTATS_VALUES);
#else
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
		{
			/* Store the PID in the final additional 4 bytes allocated */
			IMG_UINT32 *puiTemp = (IMG_UINT32*) (((IMG_BYTE*)pvAddr) + (ksize(pvAddr) - ALLOCMEM_MEMSTATS_PADDING));
			*puiTemp = OSGetCurrentClientProcessIDKM();
		}
		PVRSRVStatsIncrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_KMALLOC, ksize(pvAddr), OSGetCurrentClientProcessIDKM());
#else
		IMG_CPU_PHYADDR sCpuPAddr;
		sCpuPAddr.uiAddr = 0;

		PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_KMALLOC,
									 pvAddr,
									 sCpuPAddr,
									 ksize(pvAddr),
									 NULL,
									 OSGetCurrentClientProcessIDKM());
#endif /* !defined(PVRSRV_ENABLE_MEMORY_STATS) */
#endif /* defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON) */
	}
	else
	{
#if defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON)
		IMG_CPU_PHYADDR sCpuPAddr;
		sCpuPAddr.uiAddr = 0;

		_PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
									  pvAddr,
									  sCpuPAddr,
									  ((ui32Size + PAGE_SIZE-1) & ~(PAGE_SIZE-1)),
									  NULL,
									  OSGetCurrentClientProcessIDKM()
									  DEBUG_MEMSTATS_VALUES);
#else
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
		PVRSRVStatsIncrMemAllocStatAndTrack(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
		                                    ((ui32Size + PAGE_SIZE-1) & ~(PAGE_SIZE-1)),
		                                    (IMG_UINT64)(uintptr_t) pvAddr,
		                                    OSGetCurrentClientProcessIDKM());
#else
		IMG_CPU_PHYADDR sCpuPAddr;
		sCpuPAddr.uiAddr = 0;

		PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
									 pvAddr,
									 sCpuPAddr,
									 ((ui32Size + PAGE_SIZE-1) & ~(PAGE_SIZE-1)),
									 NULL,
									 OSGetCurrentClientProcessIDKM());
#endif /* !defined(PVRSRV_ENABLE_MEMORY_STATS) */
#endif /* defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON) */
	}
#endif /* !defined(PVRSRV_ENABLE_PROCESS_STATS) */
}

static inline void _pvr_alloc_stats_remove(void *pvAddr)
{
#if !defined(PVRSRV_ENABLE_PROCESS_STATS)
	PVR_UNREFERENCED_PARAMETER(pvAddr);
#else
	if (!is_vmalloc_addr(pvAddr))
	{
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
		{
			IMG_UINT32 *puiTemp = (IMG_UINT32*) (((IMG_BYTE*)pvAddr) + (ksize(pvAddr) - ALLOCMEM_MEMSTATS_PADDING));
			PVRSRVStatsDecrMemKAllocStat(ksize(pvAddr), *puiTemp);
		}
#else
		PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_KMALLOC,
		                                (IMG_UINT64)(uintptr_t) pvAddr,
		                                OSGetCurrentClientProcessIDKM());
#endif
	}
	else
	{
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
		PVRSRVStatsDecrMemAllocStatAndUntrack(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
		                                      (IMG_UINT64)(uintptr_t) pvAddr);
#else
		PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_VMALLOC,
		                                (IMG_UINT64)(uintptr_t) pvAddr,
		                                OSGetCurrentClientProcessIDKM());
#endif
	}
#endif /* !defined(PVRSRV_ENABLE_PROCESS_STATS) */
}

#if defined(PVRSRV_ENABLE_PROCESS_STATS) && defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON)
void *_OSAllocMem(IMG_UINT32 ui32Size DEBUG_MEMSTATS_PARAMS)
#else
void *OSAllocMem(IMG_UINT32 ui32Size)
#endif
{
	void *pvRet = NULL;

	if ((ui32Size + ALLOCMEM_MEMSTATS_PADDING) > PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD)
	{
		pvRet = vmalloc(ui32Size);
	}
	if (pvRet == NULL)
	{
		pvRet = kmalloc(ui32Size + ALLOCMEM_MEMSTATS_PADDING, GFP_KERNEL);
	}

	if (pvRet != NULL)
	{
		_pvr_alloc_stats_add(pvRet, ui32Size DEBUG_MEMSTATS_VALUES);
	}

	return pvRet;
}

#if defined(PVRSRV_ENABLE_PROCESS_STATS) && defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS_ON)
void *_OSAllocZMem(IMG_UINT32 ui32Size DEBUG_MEMSTATS_PARAMS)
#else
void *OSAllocZMem(IMG_UINT32 ui32Size)
#endif
{
	void *pvRet = NULL;

	if ((ui32Size + ALLOCMEM_MEMSTATS_PADDING) > PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD)
	{
		pvRet = vzalloc(ui32Size);
	}
	if (pvRet == NULL)
	{
		pvRet = kzalloc(ui32Size + ALLOCMEM_MEMSTATS_PADDING, GFP_KERNEL);
	}

	if (pvRet != NULL)
	{
		_pvr_alloc_stats_add(pvRet, ui32Size DEBUG_MEMSTATS_VALUES);
	}

	return pvRet;
}

/** NEW FUNC ENDS HERE **/

/*
 * The parentheses around OSFreeMem prevent the macro in allocmem.h from
 * applying, as it would break the function's definition.
 */
void (OSFreeMem)(void *pvMem)
{
	if (pvMem != NULL)
	{
		_pvr_alloc_stats_remove(pvMem);

		if (!is_vmalloc_addr(pvMem))
		{
			_pvr_kfree(pvMem);
		}
		else
		{
			_pvr_vfree(pvMem);
		}
	}
}

void *OSAllocMemNoStats(IMG_UINT32 ui32Size)
{
	void *pvRet = NULL;

	if (ui32Size > PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD)
	{
		pvRet = vmalloc(ui32Size);
	}
	if (pvRet == NULL)
	{
		pvRet = kmalloc(ui32Size, GFP_KERNEL);
	}

	return pvRet;
}

void *OSAllocZMemNoStats(IMG_UINT32 ui32Size)
{
	void *pvRet = NULL;

	if (ui32Size > PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD)
	{
		pvRet = vzalloc(ui32Size);
	}
	if (pvRet == NULL)
	{
		pvRet = kzalloc(ui32Size, GFP_KERNEL);
	}

	return pvRet;
}

/*
 * The parentheses around OSFreeMemNoStats prevent the macro in allocmem.h from
 * applying, as it would break the function's definition.
 */
void (OSFreeMemNoStats)(void *pvMem)
{
	if (pvMem != NULL)
	{
		if ( !is_vmalloc_addr(pvMem) )
		{
			_pvr_kfree(pvMem);
		}
		else
		{
			_pvr_vfree(pvMem);
		}
	}
}
