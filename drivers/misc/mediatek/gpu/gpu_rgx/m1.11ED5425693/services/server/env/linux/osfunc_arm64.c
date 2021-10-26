/*************************************************************************/ /*!
@File
@Title          arm specific OS functions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    OS functions who's implementation are processor specific
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
#include <linux/cpumask.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#include "pvrsrv_error.h"
#include "img_types.h"
#include "img_defs.h"
#include "osfunc.h"
#include "pvr_debug.h"

#if defined(CONFIG_OUTER_CACHE)
  /* If you encounter a 64-bit ARM system with an outer cache, you'll need
   * to add the necessary code to manage that cache. See osfunc_arm.c
   * for an example of how to do so.
   */
	#error "CONFIG_OUTER_CACHE not supported on arm64."
#endif

static void per_cpu_cache_flush(void *arg)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,2,0))
	unsigned long irqflags;
	signed long Clidr, Csselr, LoC, Assoc, Nway, Nsets, Level, Lsize, Var;
	static DEFINE_SPINLOCK(spinlock);

	spin_lock_irqsave(&spinlock, irqflags);

	/* Read cache level ID register */
	asm volatile (
		"dmb sy\n\t"
		"mrs %[rc], clidr_el1\n\t"
		: [rc] "=r" (Clidr));

	/* Exit if there is no cache level of coherency */
	LoC = (Clidr & (((1UL << 3)-1) << 24)) >> 23;
	if (! LoC)
	{
		goto e0;
	}

	/*
		This walks the cache hierarchy until the LLC/LOC cache, at each level skip
		only instruction caches and determine the attributes at this dcache level.
	*/
	for (Level = 0; LoC > Level; Level += 2)
	{
		/* Mask off this CtypeN bit, skip if not unified cache or separate
		   instruction and data caches */
		Var = (Clidr >> (Level + (Level >> 1))) & ((1UL << 3) - 1);
		if (Var < 2)
		{
			continue;
		}

		/* Select this dcache level for query */
		asm volatile (
			"msr csselr_el1, %[val]\n\t"
			"isb\n\t"
			"mrs %[rc], ccsidr_el1\n\t"
			: [rc] "=r" (Csselr) : [val] "r" (Level));

		/* Look-up this dcache organisation attributes */
		Nsets = (Csselr >> 13) & ((1UL << 15) - 1);
		Assoc = (Csselr >> 3) & ((1UL << 10) - 1);
		Lsize = (Csselr & ((1UL << 3) - 1)) + 4;
		Nway = 0;

		/* For performance, do these in assembly; foreach dcache level/set,
		   foreach dcache set/way, construct the "DC CISW" instruction
		   argument and issue instruction */
		asm volatile (
			"mov x6, %[val0]\n\t"
			"mov x9, %[rc1]\n\t"
			"clz w9, w6\n\t"
			"mov %[rc1], x9\n\t"
			"lsetloop:\n\t"
			"mov %[rc5], %[val0]\n\t"
			"swayloop:\n\t"
			"lsl x6, %[rc5], %[rc1]\n\t"
			"orr x9, %[val2], x6\n\t"
			"lsl x6, %[rc3], %[val4]\n\t"
			"orr x9, x9, x6\n\t"
			"dc	cisw, x9\n\t"
			"subs %[rc5], %[rc5], #1\n\t"
			"b.ge swayloop\n\t"
			"subs %[rc3], %[rc3], #1\n\t"
			"b.ge lsetloop\n\t"
			: [rc1] "+r" (Nway), [rc3] "+r" (Nsets), [rc5] "+r" (Var)
			: [val0] "r" (Assoc), [val2] "r" (Level), [val4] "r" (Lsize)
			: "x6", "x9", "cc");
	}

e0:
	/* Re-select L0 d-cache as active level, issue barrier before exit */
	Var = 0;
	asm volatile (
		"msr csselr_el1, %[val]\n\t"
		"dsb sy\n\t"
		"isb\n\t"
		: : [val] "r" (Var));

	spin_unlock_irqrestore(&spinlock, irqflags);
#else
	/* Use MTK internal cahce flush */
	/* flush_cache_all(); */
	__inner_flush_dcache_all();
#endif
	PVR_UNREFERENCED_PARAMETER(arg);
}

static inline void FlushRange(void *pvRangeAddrStart,
							  void *pvRangeAddrEnd,
							  PVRSRV_CACHE_OP eCacheOp)
{
	IMG_UINT32 ui32CacheLineSize = OSCPUCacheAttributeSize(PVR_DCACHE_LINE_SIZE);
	IMG_BYTE *pbStart = pvRangeAddrStart;
	IMG_BYTE *pbEnd = pvRangeAddrEnd;
	IMG_BYTE *pbBase;

	/*
	  On arm64, the TRM states in D5.8.1 (data and unified caches) that if cache
	  maintenance is performed on a memory location using a VA, the effect of
	  that cache maintenance is visible to all VA aliases of the physical memory
	  location. So here it's quicker to issue the machine cache maintenance
	  instruction directly without going via the Linux kernel DMA framework as
	  this is sufficient to maintain the CPU d-caches on arm64.
	 */
	pbEnd = (IMG_BYTE *) PVR_ALIGN((uintptr_t)pbEnd, (uintptr_t)ui32CacheLineSize);
	for (pbBase = pbStart; pbBase < pbEnd; pbBase += ui32CacheLineSize)
	{
		switch (eCacheOp)
		{
			case PVRSRV_CACHE_OP_CLEAN:
				asm volatile ("dc cvac, %0" :: "r" (pbBase));
				break;

			case PVRSRV_CACHE_OP_INVALIDATE:
				asm volatile ("dc ivac, %0" :: "r" (pbBase));
				break;

			case PVRSRV_CACHE_OP_FLUSH:
				asm volatile ("dc civac, %0" :: "r" (pbBase));
				break;

			default:
				PVR_DPF((PVR_DBG_ERROR,
						"%s: Cache maintenance operation type %d is invalid",
						__func__, eCacheOp));
				break;
		}
	}
}

PVRSRV_ERROR OSCPUOperation(PVRSRV_CACHE_OP uiCacheOp)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	switch (uiCacheOp)
	{
		case PVRSRV_CACHE_OP_CLEAN:
		case PVRSRV_CACHE_OP_FLUSH:
		case PVRSRV_CACHE_OP_INVALIDATE:
			on_each_cpu(per_cpu_cache_flush, NULL, 1);
			break;

		case PVRSRV_CACHE_OP_NONE:
			break;

		default:
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Global cache operation type %d is invalid",
					__func__, uiCacheOp));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			PVR_ASSERT(0);
			break;
	}

	return eError;
}

void OSCPUCacheFlushRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
							void *pvVirtStart,
							void *pvVirtEnd,
							IMG_CPU_PHYADDR sCPUPhysStart,
							IMG_CPU_PHYADDR sCPUPhysEnd)
{
	struct device *dev;
	const struct dma_map_ops *dma_ops;

	if (pvVirtStart)
	{
		FlushRange(pvVirtStart, pvVirtEnd, PVRSRV_CACHE_OP_FLUSH);
		return;
	}

	dev = psDevNode->psDevConfig->pvOSDevice;

	dma_ops = get_dma_ops(dev);
	dma_ops->sync_single_for_device(dev, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_TO_DEVICE);
	dma_ops->sync_single_for_cpu(dev, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_FROM_DEVICE);
}

void OSCPUCacheCleanRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
							void *pvVirtStart,
							void *pvVirtEnd,
							IMG_CPU_PHYADDR sCPUPhysStart,
							IMG_CPU_PHYADDR sCPUPhysEnd)
{
	struct device *dev;
	const struct dma_map_ops *dma_ops;

	if (pvVirtStart)
	{
		FlushRange(pvVirtStart, pvVirtEnd, PVRSRV_CACHE_OP_CLEAN);
		return;
	}

	dev = psDevNode->psDevConfig->pvOSDevice;

	dma_ops = get_dma_ops(psDevNode->psDevConfig->pvOSDevice);
	dma_ops->sync_single_for_device(dev, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_TO_DEVICE);
}

void OSCPUCacheInvalidateRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
								 void *pvVirtStart,
								 void *pvVirtEnd,
								 IMG_CPU_PHYADDR sCPUPhysStart,
								 IMG_CPU_PHYADDR sCPUPhysEnd)
{
	struct device *dev;
	const struct dma_map_ops *dma_ops;

	if (pvVirtStart)
	{
		FlushRange(pvVirtStart, pvVirtEnd, PVRSRV_CACHE_OP_INVALIDATE);
		return;
	}

	dev = psDevNode->psDevConfig->pvOSDevice;

	dma_ops = get_dma_ops(psDevNode->psDevConfig->pvOSDevice);
	dma_ops->sync_single_for_cpu(dev, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_FROM_DEVICE);
}

PVRSRV_CACHE_OP_ADDR_TYPE OSCPUCacheOpAddressType(void)
{
	return PVRSRV_CACHE_OP_ADDR_TYPE_PHYSICAL;
}

void OSUserModeAccessToPerfCountersEn(void)
{
	/* FIXME: implement similarly to __arm__ */
}

IMG_BOOL OSIsWriteCombineUnalignedSafe(void)
{
	/*
	 * Under ARM64 there is the concept of 'device' [0] and 'normal' [1] memory.
	 * Unaligned access on device memory is explicitly disallowed [2]:
	 *
	 * 'Further, unaligned accesses are only allowed to regions marked as Normal
	 *  memory type.
	 *  ...
	 *  Attempts to perform unaligned accesses when not allowed will cause an
	 *  alignment fault (data abort).'
	 *
	 * Write-combine on ARM64 can be implemented as either normal non-cached
	 * memory (NORMAL_NC) or as device memory with gathering enabled
	 * (DEVICE_GRE.) Kernel 3.13 changed this from the latter to the former.
	 *
	 * [0]:http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.den0024a/CHDBDIDF.html
	 * [1]:http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.den0024a/ch13s01s01.html
	 * [2]:http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka15414.html
	 */

	pgprot_t pgprot = pgprot_writecombine(PAGE_KERNEL);

	return (pgprot_val(pgprot) & PTE_ATTRINDX_MASK) == PTE_ATTRINDX(MT_NORMAL_NC);
}
