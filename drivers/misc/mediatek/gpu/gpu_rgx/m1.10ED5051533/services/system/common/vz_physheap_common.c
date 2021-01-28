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

PVRSRV_ERROR SysVzCreateDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;

	eError = PvzClientCreateDevPhysHeaps(psDevConfig, 0);
	PVR_LOG_IF_ERROR(eError, "PvzClientCreateDevPhysHeaps");

	return eError;
}

void SysVzDestroyDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PvzClientDestroyDevPhysHeaps(psDevConfig, 0);
}

PVRSRV_ERROR SysVzRegisterFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;
	PVRSRV_DEVICE_PHYS_HEAP eHeap = PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL;

	eError = SysVzGetPhysHeapOrigin(psDevConfig, eHeap, &eHeapOrigin);
	PVR_LOGG_IF_ERROR(eError, "SysVzGetPhysHeapOrigin", e0);

	if (eHeapOrigin != PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
	{
		PHYS_HEAP_CONFIG *psPhysHeapConfig;
		IMG_DEV_PHYADDR sDevPAddr;
		IMG_UINT64 ui64DevPSize;

		psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, eHeap);
		PVR_LOGR_IF_FALSE((NULL != psPhysHeapConfig), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INVALID_PARAMS);

		sDevPAddr.uiAddr = psPhysHeapConfig->pasRegions[0].sStartAddr.uiAddr;
		PVR_LOGR_IF_FALSE((0 != sDevPAddr.uiAddr), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INVALID_PARAMS);
		ui64DevPSize = psPhysHeapConfig->pasRegions[0].uiSize;
		PVR_LOGR_IF_FALSE((0 != ui64DevPSize), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INVALID_PARAMS);

		eError = PvzClientMapDevPhysHeap(psDevConfig, 0, sDevPAddr, ui64DevPSize);
		PVR_LOGG_IF_ERROR(eError, "PvzClientMapDevPhysHeap", e0);
	}

e0:
	return eError;
}

PVRSRV_ERROR SysVzUnregisterFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;
	PVRSRV_DEVICE_PHYS_HEAP eHeapType = PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL;

	eError = SysVzGetPhysHeapOrigin(psDevConfig, eHeapType, &eHeapOrigin);
	PVR_LOGG_IF_ERROR(eError, "PvzClientMapDevPhysHeap", e0);

	if (eHeapOrigin != PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
	{
		eError = PvzClientUnmapDevPhysHeap(psDevConfig, 0);
		PVR_LOGG_IF_ERROR(eError, "PvzClientMapDevPhysHeap", e0);
	}

e0:
	return eError;
}

PVRSRV_ERROR SysVzRegisterPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
								   PVRSRV_DEVICE_PHYS_HEAP eHeap)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PHYS_HEAP_CONFIG *psPhysHeapConfig;
	PVR_LOGR_IF_FALSE((eHeap < PVRSRV_DEVICE_PHYS_HEAP_LAST), "Invalid Heap", PVRSRV_ERROR_INVALID_PARAMS);
	PVR_LOGR_IF_FALSE((eHeap != PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL), "Skipping CPU local heap registration", PVRSRV_OK);

	/* Currently we only support GPU/FW DMA physheap registration */
	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, eHeap);
	PVR_LOGR_IF_FALSE((NULL != psPhysHeapConfig), "SysVzGetPhysHeapConfig", PVRSRV_ERROR_INVALID_PARAMS);

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

PVRSRV_ERROR  SysVzSetPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
									   PVRSRV_DEVICE_PHYS_HEAP ePhysHeap,
									   PHYS_HEAP_TYPE eHeapType,
									   IMG_DEV_PHYADDR sPhysHeapAddr,
									   IMG_UINT64 ui64PhysHeapSize)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_INVALID_PARAMS;
	PHYS_HEAP_CONFIG *psPhysHeapConfig;

	psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, ePhysHeap);
	PVR_LOGR_IF_FALSE((psPhysHeapConfig != NULL), "Invalid PhysHeapConfig", eError);
	PVR_LOGR_IF_FALSE((ui64PhysHeapSize != 0), "Invalid PhysHeapSize", eError);

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

			PVR_ASSERT(! psPhysHeapConfig->bDynAlloc);
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
	IMG_UINT64 uiAddr;
	PVRSRV_ERROR eError;
	VMM_PVZ_CONNECTION *psVmmPvz;

	PVR_UNREFERENCED_PARAMETER(eHeapType);

	psVmmPvz = SysVzPvzConnectionAcquire();
	PVR_ASSERT(psVmmPvz);

	PVR_ASSERT(psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapAddrSize);

	eError = psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapAddrSize(psDevConfig,
																ePhysHeap,
																pui64Size,
																&uiAddr);
	if (eError != PVRSRV_OK)
	{
		if (eError == PVRSRV_ERROR_NOT_IMPLEMENTED)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: VMM/PVZ pfnGetDevPhysHeapAddrSize() must be implemented (%s)",
					__FUNCTION__,
					PVRSRVGetErrorStringKM(eError)));
		}

		goto e0;
	}

	psAddr->uiAddr = uiAddr;
e0:
	SysVzPvzConnectionRelease(psVmmPvz);
	return eError;
}

PVRSRV_ERROR SysVzGetPhysHeapOrigin(PVRSRV_DEVICE_CONFIG *psDevConfig,
									PVRSRV_DEVICE_PHYS_HEAP eHeap,
									PVRSRV_DEVICE_PHYS_HEAP_ORIGIN *peOrigin)
{
	PVRSRV_ERROR eError;
	VMM_PVZ_CONNECTION *psVmmPvz;

	psVmmPvz = SysVzPvzConnectionAcquire();
	PVR_ASSERT(psVmmPvz);

	PVR_ASSERT(psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapOrigin);

	eError = psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapOrigin(psDevConfig,
															  eHeap,
															  peOrigin);
	if (eError != PVRSRV_OK)
	{
		if (eError == PVRSRV_ERROR_NOT_IMPLEMENTED)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: VMM/PVZ pfnGetDevPhysHeapOrigin() must be implemented (%s)",
					__FUNCTION__,
					PVRSRVGetErrorStringKM(eError)));
		}

		goto e0;
	}

e0:
	SysVzPvzConnectionRelease(psVmmPvz);
	return eError;
}

PVRSRV_ERROR SysVzPvzCreateDevPhysHeaps(IMG_UINT32 ui32OSID,
										IMG_UINT32 ui32DevID,
										IMG_UINT32 *pePhysHeapType,
										IMG_UINT64 *pui64FwPhysHeapSize,
										IMG_UINT64 *pui64FwPhysHeapAddr,
										IMG_UINT64 *pui64GpuPhysHeapSize,
										IMG_UINT64 *pui64GpuPhysHeapAddr)
{
	IMG_UINT64 uiHeapSize;
	IMG_DEV_PHYADDR sCardBase;
	IMG_CPU_PHYADDR sStartAddr;
	PHYS_HEAP_CONFIG *psPhysHeap;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	PVRSRV_DEVICE_PHYS_HEAP ePhysHeap;
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR eError = PVRSRV_ERROR_INVALID_PARAMS;
	PVR_LOGR_IF_FALSE((ui32DevID == 0), "Invalid Device ID", eError);
	PVR_LOGR_IF_FALSE((psPVRSRVData != NULL), "Invalid PVRSRVData", eError);
	PVR_LOGR_IF_FALSE((ui32OSID > 0 && ui32OSID < RGXFW_NUM_OS), "Invalid OSID", eError);

	/* For now, limit support to single device setups */
	psDeviceNode = psPVRSRVData->psDeviceNodeList;
	psDevConfig = psDeviceNode->psDevConfig;

	/* Default is a kernel managed UMA 
	   physheap memory configuration */
	*pui64FwPhysHeapSize = (IMG_UINT64)0;
	*pui64FwPhysHeapAddr = (IMG_UINT64)0;
	*pui64GpuPhysHeapSize = (IMG_UINT64)0;
	*pui64GpuPhysHeapAddr = (IMG_UINT64)0;

	*pePhysHeapType = (IMG_UINT32) SysVzGetMemoryConfigPhysHeapType();
	for (ePhysHeap = 0; ePhysHeap < PVRSRV_DEVICE_PHYS_HEAP_LAST; ePhysHeap++)
	{
		switch (ePhysHeap)
		{
			/* Only interested in these physheaps */
			case PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL:
			case PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL:
				{
					PVRSRV_ERROR eError;

					eError = SysVzGetPhysHeapOrigin(psDevConfig,
													ePhysHeap,
													&eHeapOrigin);
					PVR_LOGR_IF_ERROR(eError, "SysVzGetPhysHeapOrigin");

					if (eHeapOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST)
					{
						continue;
					}
				}
				break;

			default:
				continue;
		}

		/* Determine what type of physheap backs this phyconfig */
		psPhysHeap = SysVzGetPhysHeapConfig(psDevConfig, ePhysHeap);
		if (psPhysHeap && psPhysHeap->pasRegions)
		{
			/* Services managed physheap (LMA/UMA-carve-out/DMA) */
			sStartAddr = psPhysHeap->pasRegions[0].sStartAddr;
			sCardBase = psPhysHeap->pasRegions[0].sCardBase;
			uiHeapSize = psPhysHeap->pasRegions[0].uiSize;

			if (! uiHeapSize)
			{
				/* UMA (i.e. non carve-out), don't re-base so skip */
				PVR_ASSERT(!sStartAddr.uiAddr && !sCardBase.uiAddr);
				continue;
			}

			/* Rebase this guest OSID physical heap */
			sStartAddr.uiAddr += ui32OSID * uiHeapSize;
			sCardBase.uiAddr += ui32OSID * uiHeapSize;

			switch (ePhysHeap)
			{
				case PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL:
					*pui64GpuPhysHeapSize = uiHeapSize;
					*pui64GpuPhysHeapAddr = sStartAddr.uiAddr;
					break;

				case PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL:
					*pui64FwPhysHeapSize = uiHeapSize;
					*pui64FwPhysHeapAddr = sStartAddr.uiAddr;
					break;

				default:
					PVR_ASSERT(0);
					break;
			}
		}
		else
		{
#if defined(DEBUG)
			eError = SysVzGetPhysHeapOrigin(psDevConfig,
											ePhysHeap,
											&eHeapOrigin);
			PVR_LOGR_IF_ERROR(eError, "SysVzGetPhysHeapOrigin");

			if (eHeapOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
			{
				PVR_ASSERT(ePhysHeap != PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL);
			}
#endif
		}
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR SysVzPvzDestroyDevPhysHeaps(IMG_UINT32 ui32OSID, IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32OSID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	return PVRSRV_OK;
}

PVRSRV_ERROR SysVzPvzRegisterFwPhysHeap(IMG_UINT32 ui32OSID,
										IMG_UINT32 ui32DevID,
										IMG_UINT64 ui64Size,
										IMG_UINT64 ui64PAddr)
{
	PVRSRV_DEVICE_NODE* psDeviceNode;
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR eError = PVRSRV_ERROR_INVALID_PARAMS;
	PVRSRV_DEVICE_PHYS_HEAP eHeapType = PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL;
	PVR_LOGR_IF_FALSE((ui32DevID == 0), "Invalid Device ID", eError);
	PVR_LOGR_IF_FALSE((psPVRSRVData != NULL), "Invalid PVRSRVData", eError);

	psDeviceNode = psPVRSRVData->psDeviceNodeList;
	psDevConfig = psDeviceNode->psDevConfig;

	eError = SysVzGetPhysHeapOrigin(psDevConfig,
									eHeapType,
									&eHeapOrigin);
	PVR_LOGG_IF_ERROR(eError, "SysVzGetPhysHeapOrigin", e0);

#if defined(SUPPORT_RGX)
	if (eHeapOrigin != PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
	{
		IMG_DEV_PHYADDR sDevPAddr = {ui64PAddr};
		eError = RGXVzRegisterFirmwarePhysHeap(psDeviceNode,
											   ui32OSID,
											   sDevPAddr,
											   ui64Size);
		PVR_LOGG_IF_ERROR(eError, "RGXVzRegisterFirmwarePhysHeap", e0);
	}
#else
	PVR_UNREFERENCED_PARAMETER(ui32OSID);
	PVR_UNREFERENCED_PARAMETER(ui64Size);
	PVR_UNREFERENCED_PARAMETER(ui64PAddr);
#endif

e0:
	return eError;
}

PVRSRV_ERROR SysVzPvzUnregisterFwPhysHeap(IMG_UINT32 ui32OSID, IMG_UINT32 ui32DevID)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR eError = PVRSRV_ERROR_INVALID_PARAMS;
	PVRSRV_DEVICE_PHYS_HEAP eHeap = PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL;
	PVR_LOGR_IF_FALSE((ui32DevID == 0), "Invalid Device ID", eError);
	PVR_LOGR_IF_FALSE((psPVRSRVData != NULL), "Invalid PVRSRVData", eError);

	psDeviceNode = psPVRSRVData->psDeviceNodeList;
	psDevConfig = psDeviceNode->psDevConfig;

	eError = SysVzGetPhysHeapOrigin(psDevConfig,
									eHeap,
									&eHeapOrigin);
	PVR_LOGG_IF_ERROR(eError, "SysVzGetPhysHeapOrigin", e0);

#if defined(SUPPORT_RGX)
	if (eHeapOrigin != PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
	{
		psDeviceNode = psPVRSRVData->psDeviceNodeList;
		eError = RGXVzUnregisterFirmwarePhysHeap(psDeviceNode, ui32OSID);
		PVR_LOG_IF_ERROR(eError, "RGXVzUnregisterFirmwarePhysHeap");
	}
#else
	PVR_UNREFERENCED_PARAMETER(ui32OSID);
#endif

e0:
	return eError;
}

/******************************************************************************
 End of file (vz_physheap_common.c)
******************************************************************************/
