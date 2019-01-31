/*************************************************************************/ /*!
@File
@Title          mips specific OS functions
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
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#include "pvrsrv_error.h"
#include "img_types.h"
#include "osfunc.h"
#include "pvr_debug.h"

PVRSRV_ERROR OSCPUOperation(PVRSRV_CACHE_OP uiCacheOp)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	switch(uiCacheOp)
	{
		case PVRSRV_CACHE_OP_CLEAN:
		case PVRSRV_CACHE_OP_FLUSH:
		case PVRSRV_CACHE_OP_INVALIDATE:
			eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
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

void OSCPUCacheFlushRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
                            void *pvVirtStart,
                            void *pvVirtEnd,
                            IMG_CPU_PHYADDR sCPUPhysStart,
                            IMG_CPU_PHYADDR sCPUPhysEnd)
{
	unsigned long len;
	PVR_UNREFERENCED_PARAMETER(sCPUPhysStart);
	PVR_UNREFERENCED_PARAMETER(sCPUPhysEnd);

	PVR_ASSERT((uintptr_t) pvVirtEnd >= (uintptr_t) pvVirtEnd);

	len = (unsigned long) pvVirtEnd - (unsigned long) pvVirtStart;
	dma_cache_sync(psDevNode->psDevConfig->pvOSDevice, (void *)pvVirtStart, len, DMA_BIDIRECTIONAL);
}

void OSCPUCacheCleanRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
                            void *pvVirtStart,
                            void *pvVirtEnd,
                            IMG_CPU_PHYADDR sCPUPhysStart,
                            IMG_CPU_PHYADDR sCPUPhysEnd)
{
	unsigned long len;
	PVR_UNREFERENCED_PARAMETER(sCPUPhysStart);
	PVR_UNREFERENCED_PARAMETER(sCPUPhysEnd);

	PVR_ASSERT((uintptr_t) pvVirtEnd >= (uintptr_t) pvVirtEnd);

	len = (unsigned long) pvVirtEnd - (unsigned long) pvVirtStart;
	dma_cache_sync(psDevNode->psDevConfig->pvOSDevice, (void *)pvVirtStart, len, DMA_TO_DEVICE);
}

void OSCPUCacheInvalidateRangeKM(PVRSRV_DEVICE_NODE *psDevNode,
                                 void *pvVirtStart,
                                 void *pvVirtEnd,
                                 IMG_CPU_PHYADDR sCPUPhysStart,
                                 IMG_CPU_PHYADDR sCPUPhysEnd)
{
	unsigned long len;
	PVR_UNREFERENCED_PARAMETER(sCPUPhysStart);
	PVR_UNREFERENCED_PARAMETER(sCPUPhysEnd);

	PVR_ASSERT((uintptr_t) pvVirtEnd >= (uintptr_t) pvVirtEnd);

	len = (unsigned long) pvVirtEnd - (unsigned long) pvVirtStart;
	dma_cache_sync(psDevNode->psDevConfig->pvOSDevice, (void *)pvVirtStart, len, DMA_FROM_DEVICE);
}

PVRSRV_CACHE_OP_ADDR_TYPE OSCPUCacheOpAddressType(void)
{
	return PVRSRV_CACHE_OP_ADDR_TYPE_VIRTUAL;
}

void OSUserModeAccessToPerfCountersEn(void)
{
	/* Not applicable to MIPS architecture. */
}
