/*************************************************************************/ /*!
@File           vz_physheap_common.c
@Title          System virtualization common physheap configuration API(s)
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System virtualization common physical heap configuration API(s)
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
#include "img_defs.h"
#include "physheap.h"
#include "rgxdevice.h"
#include "pvrsrv_device.h"
#include "rgxfwutils.h"

#include "dma_support.h"
#include "vz_support.h"
#include "vz_vmm_pvz.h"
#include "vz_physheap.h"
#include "vmm_pvz_client.h"
#include "vmm_impl.h"

#if !defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
PVRSRV_ERROR SysVzRegisterFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;

	PHYS_HEAP_CONFIG *psPhysHeapConfig;
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_UINT64 ui64DevPSize;

	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL);
	PVR_LOG_RETURN_IF_FALSE((NULL != psPhysHeapConfig), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INVALID_PARAMS);
	PVR_LOG_RETURN_IF_FALSE((NULL != psPhysHeapConfig->pasRegions), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INTERNAL_ERROR);

	sDevPAddr.uiAddr = psPhysHeapConfig->pasRegions[0].sStartAddr.uiAddr;
	PVR_LOG_RETURN_IF_FALSE((0 != sDevPAddr.uiAddr), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INVALID_PARAMS);
	ui64DevPSize = psPhysHeapConfig->pasRegions[0].uiSize;
	PVR_LOG_RETURN_IF_FALSE((0 != ui64DevPSize), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INVALID_PARAMS);

	eError = PvzClientMapDevPhysHeap(psDevConfig, 0, sDevPAddr, ui64DevPSize);
	PVR_LOG_RETURN_IF_ERROR(eError, "PvzClientMapDevPhysHeap");

	return eError;
}

PVRSRV_ERROR SysVzUnregisterFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	return PvzClientUnmapDevPhysHeap(psDevConfig, 0);
}
#endif

PVRSRV_ERROR SysVzRegisterPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
								   PVRSRV_DEVICE_PHYS_HEAP eHeap)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PHYS_HEAP_CONFIG *psPhysHeapConfig;
	PVR_LOG_RETURN_IF_FALSE((eHeap < PVRSRV_DEVICE_PHYS_HEAP_LAST), "Invalid Heap", PVRSRV_ERROR_INVALID_PARAMS);
	PVR_LOG_RETURN_IF_FALSE((eHeap != PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL), "Skipping CPU local heap registration", PVRSRV_OK);

	/* Currently we only support GPU/FW DMA physheap registration */
	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, eHeap);
	PVR_LOG_RETURN_IF_FALSE((NULL != psPhysHeapConfig), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INVALID_PARAMS);

	if (psPhysHeapConfig &&
		psPhysHeapConfig->pasRegions &&
		psPhysHeapConfig->pasRegions[0].hPrivData)
	{
		DMA_ALLOC *psDmaAlloc;

		if (psPhysHeapConfig->eType == PHYS_HEAP_TYPE_DMA)
		{
			/* DMA physheaps have quirks on some OS environments */
			psDmaAlloc = psPhysHeapConfig->pasRegions[0].hPrivData;
			eError = SysDmaRegisterForIoRemapping(psDmaAlloc);
			PVR_LOG_IF_ERROR(eError, "SysDmaRegisterForIoRemapping");
		}
	}

	return eError;
}

void SysVzDeregisterPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
							 PVRSRV_DEVICE_PHYS_HEAP eHeapType)
{
	PHYS_HEAP_CONFIG *psPhysHeapConfig;

	if (eHeapType == PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL ||
		eHeapType >= PVRSRV_DEVICE_PHYS_HEAP_LAST)
	{
		return;
	}

	/* Currently we only support GPU/FW physheap deregistration */
	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, eHeapType);
	PVR_LOG_IF_FALSE((psPhysHeapConfig!=NULL), "SysVzGetPhysHeapConfig");

	if (psPhysHeapConfig &&
		psPhysHeapConfig->pasRegions &&
		psPhysHeapConfig->pasRegions[0].hPrivData)
	{
		DMA_ALLOC *psDmaAlloc;

		if (psPhysHeapConfig->eType == PHYS_HEAP_TYPE_DMA)
		{
			psDmaAlloc = psPhysHeapConfig->pasRegions[0].hPrivData;
			SysDmaDeregisterForIoRemapping(psDmaAlloc);
		}
	}

}

PHYS_HEAP_CONFIG *SysVzGetPhysHeapConfig(PVRSRV_DEVICE_CONFIG *psDevConfig,
										 PVRSRV_DEVICE_PHYS_HEAP eHeapType)
{
	IMG_UINT uiIdx;
	IMG_UINT aui32PhysHeapID;
	IMG_UINT32 ui32PhysHeapCount;
	PHYS_HEAP_CONFIG *psPhysHeap;
	PHYS_HEAP_CONFIG *ps1stPhysHeap = &psDevConfig->pasPhysHeaps[0];

	if (eHeapType == PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL)
	{
		return ps1stPhysHeap;
	}

	/* Initialise here to catch lookup failures */
	ui32PhysHeapCount = psDevConfig->ui32PhysHeapCount;
	psPhysHeap = NULL;

	if (eHeapType < PVRSRV_DEVICE_PHYS_HEAP_LAST)
	{
		/* Lookup ID of the physheap and get a pointer structure */
		aui32PhysHeapID = psDevConfig->aui32PhysHeapID[eHeapType];
		for (uiIdx = 1; uiIdx < ui32PhysHeapCount; uiIdx++)
		{
			if (ps1stPhysHeap[uiIdx].ui32PhysHeapID == aui32PhysHeapID)
			{
				psPhysHeap = &ps1stPhysHeap[uiIdx];
				break;
			}
		}
	}
	PVR_LOG_IF_FALSE((psPhysHeap != NULL), "eHeapType >= PVRSRV_DEVICE_PHYS_HEAP_LAST");

	return psPhysHeap;
}

PVRSRV_ERROR SysVzSetPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
									  PVRSRV_DEVICE_PHYS_HEAP ePhysHeap,
									  PHYS_HEAP_TYPE eHeapType,
									  IMG_DEV_PHYADDR sPhysHeapAddr,
									  IMG_UINT64 ui64PhysHeapSize)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_INVALID_PARAMS;
	PHYS_HEAP_CONFIG *psPhysHeapConfig;

	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, ePhysHeap);
	PVR_LOG_RETURN_IF_FALSE((psPhysHeapConfig != NULL), "Invalid PhysHeapConfig", eError);
	PVR_LOG_RETURN_IF_FALSE((ui64PhysHeapSize != 0), "Invalid PhysHeapSize", eError);

	if (eHeapType == PHYS_HEAP_TYPE_UMA || eHeapType == PHYS_HEAP_TYPE_LMA)
	{
		/* At this junction, we _may_ initialise new state */
		PVR_ASSERT(sPhysHeapAddr.uiAddr  && ui64PhysHeapSize);

		if (psPhysHeapConfig->pasRegions == NULL)
		{
			psPhysHeapConfig->pasRegions = OSAllocZMem(sizeof(PHYS_HEAP_REGION));
			if (psPhysHeapConfig->pasRegions == NULL)
			{
				return PVRSRV_ERROR_OUT_OF_MEMORY;
			}

			psPhysHeapConfig->bDynAlloc = IMG_TRUE;
			psPhysHeapConfig->ui32NumOfRegions++;
		}

		if (eHeapType == PHYS_HEAP_TYPE_UMA)
		{
			psPhysHeapConfig->pasRegions[0].sCardBase = sPhysHeapAddr;
		}

		psPhysHeapConfig->pasRegions[0].sStartAddr.uiAddr = sPhysHeapAddr.uiAddr;
		psPhysHeapConfig->pasRegions[0].uiSize = ui64PhysHeapSize;
		psPhysHeapConfig->eType = eHeapType;

		eError = PVRSRV_OK;
	}

	PVR_LOG_IF_ERROR(eError, "SysVzSetPhysHeapAddrSize");
	return eError;
}

PVRSRV_ERROR SysVzGetPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
									  PVRSRV_DEVICE_PHYS_HEAP ePhysHeap,
									  PHYS_HEAP_TYPE eHeapType,
									  IMG_DEV_PHYADDR *psAddr,
									  IMG_UINT64 *pui64Size)
{
	PVRSRV_ERROR eError;
	VMM_PVZ_CONNECTION *psVmmPvz = SysVzPvzConnectionAcquire();

	PVR_UNREFERENCED_PARAMETER(eHeapType);

	eError = psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapAddrSize(psDevConfig,
																ePhysHeap,
																pui64Size,
																&psAddr->uiAddr);
	if (eError != PVRSRV_OK)
	{
		if (eError == PVRSRV_ERROR_NOT_IMPLEMENTED)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: VMM/PVZ pfnGetDevPhysHeapAddrSize() must be implemented (%s)",
					 __func__,
					 PVRSRVGetErrorString(eError)));
		}

		goto e0;
	}
	else if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) &&
			 ePhysHeap == PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL &&
			 (*pui64Size == (IMG_UINT64)0 && psAddr->uiAddr == (IMG_UINT64)0))
	{
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;

		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Guest PVZ config: pfnGetDevPhysHeapAddrSize(FW_LOCAL): both size/addr cannot be zero (%s)",
				__func__,
				PVRSRVGetErrorString(eError)));

		goto e0;
	}

e0:
	SysVzPvzConnectionRelease(psVmmPvz);
	return eError;
}

#if defined(SUPPORT_RGX) && !defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
PVRSRV_ERROR SysVzPvzRegisterFwPhysHeap(IMG_UINT32 ui32OSID,
										IMG_UINT32 ui32DevID,
										IMG_UINT64 ui64Size,
										IMG_UINT64 ui64PAddr)
{
	PVRSRV_DEVICE_NODE* psDeviceNode;
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR eError = PVRSRV_ERROR_INVALID_PARAMS;
	IMG_DEV_PHYADDR sDevPAddr = {ui64PAddr};

	PVR_LOG_RETURN_IF_FALSE((ui32DevID == 0), "Invalid Device ID", eError);
	PVR_LOG_RETURN_IF_FALSE((psPVRSRVData != NULL), "Invalid PVRSRVData", eError);

	psDeviceNode = psPVRSRVData->psDeviceNodeList;
	psDevConfig = psDeviceNode->psDevConfig;

	eError = RGXVzRegisterFirmwarePhysHeap(psDeviceNode,
										   ui32OSID,
										   sDevPAddr,
										   ui64Size);
	PVR_LOG_RETURN_IF_ERROR(eError, "RGXVzRegisterFirmwarePhysHeap");

	if (eError == PVRSRV_OK)
	{
		/* Invalidate MMU cache in preparation for a kick from this Guest */
		IMG_UINT32 sync;
		eError = psDeviceNode->pfnMMUCacheInvalidateKick(psDeviceNode, &sync, IMG_TRUE);
		PVR_LOG_RETURN_IF_ERROR(eError, "SysVzPvzRegisterFwPhysHeap failed to invalidate MMU cache");

		if (eError == PVRSRV_OK)
		{
			/* Everything is ready for the firmware to start interacting with this OS */
			PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
			eError = RGXFWSetFwOsState(psDevInfo, ui32OSID, RGXFWIF_OS_ONLINE);
		}
	}

	return eError;
}

PVRSRV_ERROR SysVzPvzUnregisterFwPhysHeap(IMG_UINT32 ui32OSID, IMG_UINT32 ui32DevID)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR eError = PVRSRV_ERROR_INVALID_PARAMS;

	PVR_LOG_RETURN_IF_FALSE((ui32DevID == 0), "Invalid Device ID", eError);
	PVR_LOG_RETURN_IF_FALSE((psPVRSRVData != NULL), "Invalid PVRSRVData", eError);

	psDeviceNode = psPVRSRVData->psDeviceNodeList;

	/* Order firmware to offload this OS' data and stop accepting commands from it */
	eError = RGXFWSetFwOsState(psDeviceNode->pvDevice, ui32OSID, RGXFWIF_OS_OFFLINE);
	if (eError != PVRSRV_OK)
	{
		PVR_LOG_IF_ERROR(eError, "RGXFWSetFwOsState");
		return eError;
	}

	/* it is now safe to remove the Guest's memory mappings  */
	eError = RGXVzUnregisterFirmwarePhysHeap(psDeviceNode, ui32OSID);
	PVR_LOG_IF_ERROR(eError, "RGXVzUnregisterFirmwarePhysHeap");

	return eError;
}
#endif /* defined(SUPPORT_RGX) && !defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS) */

/******************************************************************************
 End of file (vz_physheap_common.c)
******************************************************************************/
