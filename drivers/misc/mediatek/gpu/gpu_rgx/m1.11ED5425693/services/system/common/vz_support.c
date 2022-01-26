/*************************************************************************/ /*!
@File           vz_support.c
@Title          System virtualization configuration setup
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System virtualization configuration support API(s)
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
#include "pvrsrv.h"
#include "pvrsrv_device.h"

#include "dma_support.h"
#include "vz_support.h"
#include "vz_vmm_pvz.h"
#include "vz_physheap.h"
#include "vmm_pvz_client.h"
#include "vmm_pvz_server.h"

static PVRSRV_ERROR
SysVzPvzConnectionValidate(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	VMM_PVZ_CONNECTION *psVmmPvz;
	PVRSRV_ERROR eError = PVRSRV_OK;
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eOrigin = PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_LAST;
	IMG_UINT64 ui64Size = 0, ui64Addr = 0;

	/*
	 * Acquire the underlying VM manager PVZ connection & validate it.
	 */
	psVmmPvz = SysVzPvzConnectionAcquire();
	if (psVmmPvz == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: Unable to acquire PVZ connection",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e0;
	}
	else if (psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapOrigin == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: pfnGetDevPhysHeapOrigin cannot be NULL",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}
	else if (psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapAddrSize == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: pfnGetDevPhysHeapAddrSize cannot be NULL",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}
	else if (psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapAddrSize(psDevConfig,
					 	 	 	 	 	 	 	 	 	 	 	PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL,
																&ui64Size,
																&ui64Addr) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: pfnGetDevPhysHeapAddrSize(GPU) must return PVRSRV_OK",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}
	else if (psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapAddrSize(psDevConfig,
					 	 	 	 	 	 	 	 	 	 	 	PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL,
																&ui64Size,
																&ui64Addr) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: pfnGetDevPhysHeapAddrSize(FW) must return PVRSRV_OK",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}
	else if (PVRSRV_OK !=
			 psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapOrigin(psDevConfig,
															  PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL,
															  &eOrigin))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: Invalid config. function table setup\n"
				"=>: pfnGetDevPhysHeapOrigin() must return PVRSRV_OK",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}
	else if (eOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_LAST)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: Invalid config. function table setup\n"
				"=>: pfnGetDevPhysHeapOrigin() returned an invalid physheap origin",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}
	else if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST)            &&
			 eOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST &&
			 psVmmPvz->sHostFuncTab.pfnMapDevPhysHeap == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Guest PVZ config: Invalid config. function table setup\n"
				"=>: implement pfnMapDevPhysHeap() when using GUEST physheap origin",
				__func__));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}
	else if (!PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST)           &&
			 (psVmmPvz->sGuestFuncTab.pfnCreateDevConfig     == NULL ||
			  psVmmPvz->sGuestFuncTab.pfnDestroyDevConfig    == NULL ||
			  psVmmPvz->sGuestFuncTab.pfnCreateDevPhysHeaps  == NULL ||
			  psVmmPvz->sGuestFuncTab.pfnDestroyDevPhysHeaps == NULL ||
			  psVmmPvz->sGuestFuncTab.pfnMapDevPhysHeap      == NULL ||
			  psVmmPvz->sGuestFuncTab.pfnUnmapDevPhysHeap    == NULL))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Host PVZ config: Invalid guest function table setup\n",
				__func__));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}
	else if (eOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST &&
			 ui64Size == 0 &&
			 ui64Addr == 0)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: Invalid pfnGetDevPhysHeapAddrSize(FW) physheap config.\n"
				"=>: HEAP_ORIGIN_HOST is not compatible with FW UMA allocator",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}

	/* Log which PVZ setup type is being used by driver */
	if (eOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST)
	{
		/*
		 *  Static PVZ bootstrap setup
		 *
		 *  This setup uses host-origin, has no hypercall mechanism & does not support any
		 *  out-of-order initialisation of host/guest VMs/drivers. The host driver has all
		 *  the information needed to initialize all OSIDs firmware state when it's loaded
		 *  and its PVZ layer must mark all guest OSIDs as being online as part of its PVZ
		 *  initialisation. Having no out-of-order initialisation support, the guest driver
		 *  can only submit a workload to the device after the host driver has completely
		 *  initialized the firmware, the VZ hypervisor/VM setup must guarantee this.
		 */
		PVR_LOG(("Using static PVZ bootstrap setup"));
	}
	else
	{
		/*
		 *  Dynamic PVZ bootstrap setup
		 *
		 *  This setup uses guest-origin, has PVZ hypercall mechanism & supports out-of-order
		 *  initialisation of host/guest VMs/drivers. The host driver initializes only its
		 *  own OSID-0 firmware state when its loaded and each guest driver will use its PVZ
		 *  interface to hypercall to the host driver to both synchronise its initialisation
		 *  so it does not submit any workload to the firmware before the host driver has
		 *  had a chance to initialize the firmware and to also initialize its own OSID-x
		 *  firmware state.
		 */
 		PVR_LOG(("Using dynamic PVZ bootstrap setup"));
	}

e1:
	SysVzPvzConnectionRelease(psVmmPvz);
e0:
	return eError;
}

PVRSRV_ERROR SysVzDevInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;
	RGX_DATA* psDevData = psDevConfig->hDevData;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_NATIVE, PVRSRV_OK);

	/* Initialise pvz connection */
	eError =  SysVzPvzConnectionInit();
	PVR_LOGR_IF_ERROR(eError, "SysVzPvzConnectionInit");

	/* Ensure pvz connection is configured correctly */
	eError = SysVzPvzConnectionValidate(psDevConfig);
	PVR_LOGR_IF_ERROR(eError, "SysVzPvzConnectionValidate");

	psPVRSRVData->abVmOnline[0] = IMG_TRUE;
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		/* Undo any functionality not supported in guest drivers */
		psDevData->psRGXTimingInfo->bEnableRDPowIsland  = IMG_FALSE;
		psDevData->psRGXTimingInfo->bEnableActivePM = IMG_FALSE;
		psDevConfig->pfnPrePowerState  = NULL;
		psDevConfig->pfnPostPowerState = NULL;

		/* Perform additional guest-specific device
		   configuration initialisation */
		eError =  SysVzCreateDevConfig(psDevConfig);
		PVR_LOGR_IF_ERROR(eError, "SysVzCreateDevConfig");

		eError =  SysVzCreateDevPhysHeaps(psDevConfig);
		PVR_LOGR_IF_ERROR(eError, "SysVzCreateDevPhysHeaps");
	}

	/* Perform general device physheap initialisation */
	eError = SysVzInitDevPhysHeaps(psDevConfig);
	PVR_LOGR_IF_ERROR(eError, "SysVzInitDevPhysHeaps");

	return eError;
}

PVRSRV_ERROR SysVzDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_NATIVE, PVRSRV_OK);

	SysVzDeInitDevPhysHeaps(psDevConfig);
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		SysVzDestroyDevPhysHeaps(psDevConfig);
		SysVzDestroyDevConfig(psDevConfig);
	}

	SysVzPvzConnectionDeInit();
	return PVRSRV_OK;
}

PVRSRV_ERROR SysVzCreateDevConfig(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;

	eError = PvzClientCreateDevConfig(psDevConfig, 0);
	eError = (eError == PVRSRV_ERROR_NOT_IMPLEMENTED) ? PVRSRV_OK : eError;
	PVR_LOG_IF_ERROR(eError, "PvzClientCreateDevConfig");

	return eError;
}

PVRSRV_ERROR SysVzDestroyDevConfig(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;

	eError = PvzClientDestroyDevConfig(psDevConfig, 0);
	eError = (eError == PVRSRV_ERROR_NOT_IMPLEMENTED) ? PVRSRV_OK : eError;
	PVR_LOG_IF_ERROR(eError, "SysVzDestroyDevConfig");

	return eError;
}

PVRSRV_ERROR
SysVzPvzCreateDevConfig(IMG_UINT32 ui32OSID,
						IMG_UINT32 ui32DevID,
						IMG_UINT32 *pui32IRQ,
						IMG_UINT32 *pui32RegsSize,
						IMG_UINT64 *pui64RegsCpuPBase)
{
	PVRSRV_DEVICE_NODE *psDevNode;
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	if (ui32OSID == 0        ||
		ui32DevID != 0       ||
		psPVRSRVData == NULL ||
		ui32OSID >= RGXFW_NUM_OS)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* For now, limit support to single device setups */
	psDevNode = psPVRSRVData->psDeviceNodeList;
	psDevConfig = psDevNode->psDevConfig;

	/* Copy across guest VM device config information, here
	   we assume this is the same across VMs and host */
	*pui64RegsCpuPBase = psDevConfig->sRegsCpuPBase.uiAddr;
	*pui32RegsSize = psDevConfig->ui32RegsSize;
	*pui32IRQ = psDevConfig->ui32IRQ;

	return PVRSRV_OK;
}

PVRSRV_ERROR
SysVzPvzDestroyDevConfig(IMG_UINT32 ui32OSID, IMG_UINT32 ui32DevID)
{
	if (ui32OSID == 0        ||
		ui32DevID != 0       ||
		ui32OSID >= RGXFW_NUM_OS)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	return PVRSRV_OK;
}

/******************************************************************************
 End of file (vz_support.c)
******************************************************************************/
