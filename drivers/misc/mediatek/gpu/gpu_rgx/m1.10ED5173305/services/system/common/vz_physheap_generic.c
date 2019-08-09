/*************************************************************************/ /*!
@File           vz_physheap_generic.c
@Title          System virtualization physheap configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System virtualization physical heap configuration
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
#include "allocmem.h"
#include "physheap.h"
#include "rgxdevice.h"
#include "pvrsrv_device.h"
#include "rgxfwutils.h"

#include "dma_support.h"
#include "vz_support.h"
#include "vz_vmm_pvz.h"
#include "vz_physheap.h"

#if defined(CONFIG_L4)
static IMG_HANDLE gahPhysHeapIoRemap[PVRSRV_DEVICE_PHYS_HEAP_LAST];
#endif

static PVRSRV_ERROR
SysVzCreateDmaPhysHeap(PHYS_HEAP_CONFIG *psPhysHeapConfig)
{
	PVRSRV_ERROR eError;
	DMA_ALLOC *psDmaAlloc;
	PHYS_HEAP_REGION *psPhysHeapRegion;

	psPhysHeapRegion = &psPhysHeapConfig->pasRegions[0];
	PVR_LOGR_IF_FALSE((NULL != psPhysHeapRegion->hPrivData), "DMA physheap already created", PVRSRV_ERROR_INVALID_PARAMS);

	psDmaAlloc = (DMA_ALLOC*)psPhysHeapRegion->hPrivData;
	psDmaAlloc->ui64Size = psPhysHeapRegion->uiSize;

	eError = SysDmaAllocMem(psDmaAlloc);
	if (eError != PVRSRV_OK)
	{
		psPhysHeapConfig->eType = PHYS_HEAP_TYPE_UMA;
	}
	else
	{
		psPhysHeapRegion->sStartAddr.uiAddr = psDmaAlloc->sBusAddr.uiAddr;
		psPhysHeapRegion->sCardBase.uiAddr = psDmaAlloc->sBusAddr.uiAddr;
		psPhysHeapConfig->eType = PHYS_HEAP_TYPE_DMA;
	}

	return eError;
}

static void
SysVzDestroyDmaPhysHeap(PHYS_HEAP_CONFIG *psPhysHeapConfig)
{
	DMA_ALLOC *psDmaAlloc;
	PHYS_HEAP_REGION *psPhysHeapRegion;

	psPhysHeapRegion = &psPhysHeapConfig->pasRegions[0];
	psDmaAlloc = (DMA_ALLOC*)psPhysHeapRegion->hPrivData;

	if (psDmaAlloc != NULL)
	{
		PVR_LOG_IF_FALSE((0 != psPhysHeapRegion->sStartAddr.uiAddr), "Invalid DMA physheap start address");
		PVR_LOG_IF_FALSE((0 != psPhysHeapRegion->sCardBase.uiAddr), "Invalid DMA physheap card address");
		PVR_LOG_IF_FALSE((0 != psPhysHeapRegion->uiSize), "Invalid DMA physheap size");

		SysDmaFreeMem(psDmaAlloc);

		psPhysHeapRegion->sCardBase.uiAddr = 0;	
		psPhysHeapRegion->sStartAddr.uiAddr = 0;
		psPhysHeapConfig->eType = PHYS_HEAP_TYPE_UMA;
	}
}

static PVRSRV_ERROR
SysVzCreatePhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
					PVRSRV_DEVICE_PHYS_HEAP ePhysHeap)
{
	IMG_DEV_PHYADDR sHeapAddr;
	IMG_UINT64 ui64HeapSize = 0;
	PVRSRV_ERROR eError = PVRSRV_OK;
	PHYS_HEAP_REGION *psPhysHeapRegion;
	PHYS_HEAP_CONFIG *psPhysHeapConfig;
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;

	/* Lookup GPU/FW physical heap config, allocate primary region */
	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, ePhysHeap);
	PVR_LOGR_IF_FALSE((NULL != psPhysHeapConfig), "Invalid physheap config", PVRSRV_ERROR_INVALID_PARAMS);

	if (psPhysHeapConfig->pasRegions == NULL)
	{
		psPhysHeapConfig->pasRegions = OSAllocZMem(sizeof(PHYS_HEAP_REGION));
		PVR_LOGG_IF_NOMEM(psPhysHeapConfig->pasRegions, "OSAllocZMem", eError, e0);

		PVR_ASSERT(! psPhysHeapConfig->bDynAlloc);
		psPhysHeapConfig->bDynAlloc = IMG_TRUE;
		psPhysHeapConfig->ui32NumOfRegions++;
	}

	if (psPhysHeapConfig->pasRegions[0].hPrivData == NULL)
	{
		DMA_ALLOC *psDmaAlloc = OSAllocZMem(sizeof(DMA_ALLOC));
		PVR_LOGG_IF_NOMEM(psDmaAlloc, "OSAllocZMem", eError, e0);

		psDmaAlloc->pvOSDevice = psDevConfig->pvOSDevice;
		psPhysHeapConfig->pasRegions[0].hPrivData = psDmaAlloc;
	}

	/* Lookup physheap addr/size from VM manager type */
	eError = SysVzGetPhysHeapAddrSize(psDevConfig,
									  ePhysHeap,
									  PHYS_HEAP_TYPE_UMA,
								 	  &sHeapAddr,
								 	  &ui64HeapSize);
	PVR_LOGG_IF_ERROR(eError, "SysVzGetPhysHeapAddrSize", e0);

	/* Initialise physical heap and region state */
	psPhysHeapRegion = &psPhysHeapConfig->pasRegions[0];
	psPhysHeapRegion->sStartAddr.uiAddr = sHeapAddr.uiAddr;
	psPhysHeapRegion->sCardBase.uiAddr = sHeapAddr.uiAddr;
	psPhysHeapRegion->uiSize = ui64HeapSize;

	if (ePhysHeap == PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL)
	{
		/* Firmware physheaps require additional init */
		psPhysHeapConfig->pszPDumpMemspaceName = "SYSMEM";
		psPhysHeapConfig->psMemFuncs =
				psDevConfig->pasPhysHeaps[0].psMemFuncs;
	}

	/* Which driver is responsible for allocating the
	   physical memory backing the device physheap */
	eError = SysVzGetPhysHeapOrigin(psDevConfig,
									ePhysHeap,
									&eHeapOrigin);
	PVR_LOGG_IF_ERROR(eError, "SysVzGetPhysHeapOrigin", e0);

	if (psPhysHeapRegion->sStartAddr.uiAddr == 0)
	{
		if (psPhysHeapRegion->uiSize)
		{
			if (eHeapOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
			{
				/* Scale DMA size by the number of OSIDs */
				psPhysHeapRegion->uiSize *= RGXFW_NUM_OS;
			}

			eError = SysVzCreateDmaPhysHeap(psPhysHeapConfig);
			if (eError != PVRSRV_OK)
			{
				PVR_LOGG_IF_ERROR(eError, "SysVzCreateDmaPhysHeap", e0);
			}

			/* Verify the validity of DMA physheap region */
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			PVR_LOGG_IF_FALSE((0 != psPhysHeapRegion->sStartAddr.uiAddr), "Invalid DMA physheap start address", e0);
			PVR_LOGG_IF_FALSE((0 != psPhysHeapRegion->sCardBase.uiAddr), "Invalid DMA physheap card address", e0);
			PVR_LOGG_IF_FALSE((0 != psPhysHeapRegion->uiSize), "Invalid DMA physheap size", e0);
			eError = PVRSRV_OK;

			/* Services managed DMA physheap setup complete */
			psPhysHeapConfig->eType = PHYS_HEAP_TYPE_DMA;

			/* Only the PHYS_HEAP_TYPE_DMA should be registered */
			eError = SysVzRegisterPhysHeap(psDevConfig, ePhysHeap);
			if (eError != PVRSRV_OK)
			{
				PVR_LOGG_IF_ERROR(eError, "SysVzRegisterPhysHeap", e0);
			}

			if (eHeapOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
			{
				/* Restore original physheap size */
				psPhysHeapRegion->uiSize /= RGXFW_NUM_OS;
			}
		}
		else
		{
			if (psPhysHeapConfig->pasRegions[0].hPrivData)
			{
				OSFreeMem(psPhysHeapConfig->pasRegions[0].hPrivData);
				psPhysHeapConfig->pasRegions[0].hPrivData = NULL;
			}

			if (psPhysHeapConfig->bDynAlloc)
			{
				OSFreeMem(psPhysHeapConfig->pasRegions);
				psPhysHeapConfig->pasRegions = NULL;
				psPhysHeapConfig->ui32NumOfRegions--;
				psPhysHeapConfig->bDynAlloc = IMG_FALSE;
				PVR_LOGG_IF_FALSE((psPhysHeapConfig->ui32NumOfRegions == 0), "Invalid refcount", e0);
			}

			if (ePhysHeap == PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL)
			{
				/* Using UMA physheaps for FW has pre-conditions, verify */
				if (eHeapOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
				{
					PVR_DPF((PVR_DBG_ERROR,
							"%s: %s PVZ config: Invalid firmware physheap config\n"
							"=>: HOST origin (i.e. static) VZ setups require non-UMA FW physheaps spec.",
							__FUNCTION__,
							PVRSRV_VZ_MODE_IS(DRIVER_MODE_HOST) ? "Host" : "Guest"));
					eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
				}
			}

			/* Kernel managed UMA physheap setup complete */
			psPhysHeapConfig->eType = PHYS_HEAP_TYPE_UMA;
		}
	}
	else
	{
		/* Verify the validity of the UMA carve-out physheap region */
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_LOGG_IF_FALSE((0 != psPhysHeapRegion->sStartAddr.uiAddr), "Invalid UMA carve-out physheap start address", e0);
		PVR_LOGG_IF_FALSE((0 != psPhysHeapRegion->sCardBase.uiAddr), "Invalid UMA carve-out physheap card address", e0);
		PVR_LOGG_IF_FALSE((0 != psPhysHeapRegion->uiSize), "Invalid UMA carve-out physheap size", e0);
		eError = PVRSRV_OK;

		if (psPhysHeapConfig->pasRegions[0].hPrivData)
		{
			/* Need regions but don't require the DMA priv. data */
			OSFreeMem(psPhysHeapConfig->pasRegions[0].hPrivData);
			psPhysHeapConfig->pasRegions[0].hPrivData = NULL;
		}

#if defined(CONFIG_L4)
		{
			IMG_UINT64 ui64Offset;
			IMG_UINT64 ui64BaseAddr;
			IMG_CPU_VIRTADDR pvCpuVAddr;

			/* On Fiasco.OC/l4linux, ioremap physheap now */
			gahPhysHeapIoRemap[ePhysHeap] = 
							OSMapPhysToLin(psPhysHeapRegion->sStartAddr,
										   psPhysHeapRegion->uiSize,
										   PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);
			PVR_LOGG_IF_FALSE((NULL != gahPhysHeapIoRemap[ePhysHeap]), "OSMapPhysToLin", e0);
		}
#endif

		/* Services managed UMA carve-out physheap setup complete */
		psPhysHeapConfig->eType = PHYS_HEAP_TYPE_UMA;
	}

	return eError;

e0:
	if (psPhysHeapConfig->pasRegions)
	{
		SysVzDeregisterPhysHeap(psDevConfig, ePhysHeap);

		if (psPhysHeapConfig->pasRegions[0].hPrivData)
		{
			OSFreeMem(psPhysHeapConfig->pasRegions[0].hPrivData);
			psPhysHeapConfig->pasRegions[0].hPrivData = NULL;
		}

		if (psPhysHeapConfig->bDynAlloc)
		{
			OSFreeMem(psPhysHeapConfig->pasRegions);
			psPhysHeapConfig->pasRegions = NULL;
			psPhysHeapConfig->ui32NumOfRegions--;
			psPhysHeapConfig->bDynAlloc = IMG_FALSE;
			PVR_LOG_IF_FALSE((psPhysHeapConfig->ui32NumOfRegions == 0), "Invalid refcount");
		}
	}

	return  eError;
}

static void
SysVzDestroyPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
					 PVRSRV_DEVICE_PHYS_HEAP ePhysHeap)
{
	PHYS_HEAP_CONFIG *psPhysHeapConfig;

	SysVzDeregisterPhysHeap(psDevConfig, ePhysHeap);

	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, ePhysHeap);
	if (psPhysHeapConfig == NULL || 
		psPhysHeapConfig->pasRegions == NULL)
	{
		return;
	}

#if defined(CONFIG_L4)
	if (gahPhysHeapIoRemap[ePhysHeap] != NULL)
	{
		OSUnMapPhysToLin(gahPhysHeapIoRemap[ePhysHeap],
						psPhysHeapConfig->pasRegions[0].uiSize,
						PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);
	}

	gahPhysHeapIoRemap[ePhysHeap] = NULL;
#endif

	if (psPhysHeapConfig->pasRegions[0].hPrivData)
	{
		SysVzDestroyDmaPhysHeap(psPhysHeapConfig);
		OSFreeMem(psPhysHeapConfig->pasRegions[0].hPrivData);
		psPhysHeapConfig->pasRegions[0].hPrivData = NULL;
	}

	if (psPhysHeapConfig->bDynAlloc)
	{
		OSFreeMem(psPhysHeapConfig->pasRegions);
		psPhysHeapConfig->pasRegions = NULL;
		psPhysHeapConfig->ui32NumOfRegions--;
		psPhysHeapConfig->bDynAlloc = IMG_FALSE;
		PVR_LOG_IF_FALSE((psPhysHeapConfig->ui32NumOfRegions == 0), "Invalid refcount");
	}
}

static PVRSRV_ERROR
SysVzCreateGpuPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_DEVICE_PHYS_HEAP eHeap = PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL;
	return SysVzCreatePhysHeap(psDevConfig, eHeap);
}

static void
SysVzDestroyGpuPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_DEVICE_PHYS_HEAP eHeap = PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL;
	SysVzDestroyPhysHeap(psDevConfig, eHeap);
}

static PVRSRV_ERROR
SysVzCreateFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_DEVICE_PHYS_HEAP eHeap = PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL;
	return SysVzCreatePhysHeap(psDevConfig, eHeap);
}

static void
SysVzDestroyFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_DEVICE_PHYS_HEAP eHeap = PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL;
	SysVzDestroyPhysHeap(psDevConfig, eHeap);
}

PHYS_HEAP_TYPE SysVzGetMemoryConfigPhysHeapType(void)
{
	return PHYS_HEAP_TYPE_UMA;
}

PVRSRV_ERROR SysVzInitDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;

	eError = SysVzCreateFwPhysHeap(psDevConfig);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = SysVzCreateGpuPhysHeap(psDevConfig);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	return eError;
}

void SysVzDeInitDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	SysVzDestroyGpuPhysHeap(psDevConfig);
	SysVzDestroyFwPhysHeap(psDevConfig);
}

/******************************************************************************
 End of file (vz_physheap_generic.c)
******************************************************************************/
