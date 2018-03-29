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
#include "osfunc.h"
#include "pvr_debug.h"

#if defined(CONFIG_OUTER_CACHE)
  /* If you encounter a 64-bit ARM system with an outer cache, you'll need
   * to add the necessary code to manage that cache.  See osfunc_arm.c	
   * for an example of how to do so.
   */
	#error "CONFIG_OUTER_CACHE not supported on arm64."
#endif

#define ON_EACH_CPU(func, info, wait) on_each_cpu(func, info, wait)

static void per_cpu_cache_flush(void *arg)
{
	PVR_UNREFERENCED_PARAMETER(arg);
	asm volatile (
		/* Disable interrupts macro */
		".macro	disable_irq\n\t"
		"msr	daifset, #2\n\t"
		".endm\n\t"

		/* Enable interrupts macro */
		".macro	enable_irq\n\t"
		"msr	daifclr, #2\n\t"
		".endm\n\t"

		/* Save/disable interrupts macro */
		".macro	save_and_disable_irqs, olddaif\n\t"
		"mrs	\\olddaif, daif\n\t"
		"disable_irq\n\t"
		".endm\n\t"

		/* Restore/enable interrupts macro */
		".macro	restore_irqs, olddaif\n\t"
		"msr	daif, \\olddaif\n\t"
		/* "enable_irq\n\t" */
		".endm\n\t"

		/*	Flush the whole D-cache	 */
		"dmb	sy\n\t"						// ensure ordering with previous memory accesses
		"mrs	x0, clidr_el1\n\t"			// read clidr
		"and	x3, x0, #0x7000000\n\t"		// extract loc from clidr
		"lsr	x3, x3, #23\n\t"			// left align loc bit field
		"cbz	x3, finished\n\t"			// if loc is 0, then no need to clean
		"mov	x10, #0\n\t"				// start clean at cache level 0
		"loop1:\n\t"
		"add	x2, x10, x10, lsr #1\n\t"	// work out 3x current cache level
		"lsr	x1, x0, x2\n\t"				// extract cache type bits from clidr
		"and	x1, x1, #7\n\t"				// mask of the bits for current cache only
		"cmp	x1, #2\n\t"					// see what cache we have at this level
		"b.lt	skip\n\t"					// skip if no cache, or just i-cache
		"save_and_disable_irqs x9\n\t"		// make CSSELR and CCSIDR access atomic
		"msr	csselr_el1, x10\n\t"		// select current cache level in csselr
		"isb\n\t"							// isb to sych the new cssr&csidr
		"mrs	x1, ccsidr_el1\n\t"			// read the new ccsidr
		"restore_irqs x9\n\t"
		"and	x2, x1, #7\n\t"				// extract the length of the cache lines
		"add	x2, x2, #4\n\t"				// add 4 (line length offset)
		"mov	x4, #0x3ff\n\t"
		"and	x4, x4, x1, lsr #3\n\t"		// find maximum number on the way size
		"clz	w5, w4\n\t"					// find bit position of way size increment
		"mov	x7, #0x7fff\n\t"
		"and	x7, x7, x1, lsr #13\n\t"	// extract max number of the index size
		"loop2:\n\t"
		"mov	x9, x4\n\t"					// create working copy of max way size
		"loop3:\n\t"
		"lsl	x6, x9, x5\n\t"
		"orr	x11, x10, x6\n\t"			// factor way and cache number into x11
		"lsl	x6, x7, x2\n\t"
		"orr	x11, x11, x6\n\t"			// factor index number into x11
		"dc	cisw, x11\n\t"					// clean & invalidate by set/way
		"subs	x9, x9, #1\n\t"				// decrement the way
		"b.ge	loop3\n\t"
		"subs	x7, x7, #1\n\t"				// decrement the index
		"b.ge	loop2\n\t"
		"skip:\n\t"
		"add	x10, x10, #2\n\t"			// increment cache number
		"cmp	x3, x10\n\t"
		"b.gt	loop1\n\t"
		"finished:\n\t"
		"mov	x10, #0\n\t"				// switch back to cache level 0
		"msr	csselr_el1, x10\n\t"		// select current cache level in csselr
		"dsb	sy\n\t"
		"isb\n\t"
		"ret\n\t"
		:	/* no assembly to c output-variable binding */
		:	/* no support literal in inline-assembly */
		:	/* corrupted registers: x0-x7, x9-x11, cc, memory */
		"memory","cc","x0","x1","x2","x3","x4","x5","x6","x7","x9","x10","x11"
	);
}

PVRSRV_ERROR OSCPUOperation(PVRSRV_CACHE_OP uiCacheOp)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	switch(uiCacheOp)
	{
		case PVRSRV_CACHE_OP_CLEAN:
		case PVRSRV_CACHE_OP_FLUSH:
		case PVRSRV_CACHE_OP_INVALIDATE:
			ON_EACH_CPU(per_cpu_cache_flush, NULL, 1);
			break;

		case PVRSRV_CACHE_OP_NONE:
			break;

		default:
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Global cache operation type %d is invalid",
					__FUNCTION__, uiCacheOp));
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			PVR_ASSERT(0);
			break;
	}

	return eError;
}

void OSFlushCPUCacheRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
							void *pvVirtStart,
							void *pvVirtEnd,
							IMG_CPU_PHYADDR sCPUPhysStart,
							IMG_CPU_PHYADDR sCPUPhysEnd)
{
	struct dma_map_ops *dma_ops = get_dma_ops(psDevNode->psDevConfig->pvOSDevice);

	PVR_UNREFERENCED_PARAMETER(pvVirtStart);
	PVR_UNREFERENCED_PARAMETER(pvVirtEnd);

	dma_ops->sync_single_for_device(NULL, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_TO_DEVICE);
	dma_ops->sync_single_for_cpu(NULL, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_FROM_DEVICE);
}

void OSCleanCPUCacheRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
							void *pvVirtStart,
							void *pvVirtEnd,
							IMG_CPU_PHYADDR sCPUPhysStart,
							IMG_CPU_PHYADDR sCPUPhysEnd)
{
	struct dma_map_ops *dma_ops = get_dma_ops(psDevNode->psDevConfig->pvOSDevice);

	PVR_UNREFERENCED_PARAMETER(pvVirtStart);
	PVR_UNREFERENCED_PARAMETER(pvVirtEnd);

	dma_ops->sync_single_for_device(NULL, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_TO_DEVICE);
}

void OSInvalidateCPUCacheRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
								 void *pvVirtStart,
								 void *pvVirtEnd,
								 IMG_CPU_PHYADDR sCPUPhysStart,
								 IMG_CPU_PHYADDR sCPUPhysEnd)
{
	struct dma_map_ops *dma_ops = get_dma_ops(psDevNode->psDevConfig->pvOSDevice);

	PVR_UNREFERENCED_PARAMETER(pvVirtStart);
	PVR_UNREFERENCED_PARAMETER(pvVirtEnd);

	dma_ops->sync_single_for_cpu(NULL, sCPUPhysStart.uiAddr, sCPUPhysEnd.uiAddr - sCPUPhysStart.uiAddr, DMA_FROM_DEVICE);
}

PVRSRV_CACHE_OP_ADDR_TYPE OSCPUCacheOpAddressType(PVRSRV_CACHE_OP uiCacheOp)
{
	PVR_UNREFERENCED_PARAMETER(uiCacheOp);
	return PVRSRV_CACHE_OP_ADDR_TYPE_PHYSICAL;
}

void OSUserModeAccessToPerfCountersEn(void)
{
	/* FIXME: implement similarly to __arm__ */
}
