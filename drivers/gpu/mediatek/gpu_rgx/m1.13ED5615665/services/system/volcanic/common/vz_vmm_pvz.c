/*************************************************************************/ /*!
@File           vz_vmm_pvz.c
@Title          VM manager para-virtualization APIs
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    VM manager para-virtualization management
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

#include "pvrsrv.h"
#include "img_defs.h"
#include "img_types.h"
#include "pvrsrv_error.h"
#include "allocmem.h"
#include "pvrsrv.h"
#include "vz_vmm_pvz.h"

PVRSRV_ERROR SysVzPvzConnectionInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;
	VMM_PVZ_CONNECTION *psVmmPvz;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	/* Create para-virtualization connection lock */
	eError = OSLockCreate(&psPVRSRVData->hPvzConnectionLock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: OSLockCreate failed (%s)",
				__func__,
				PVRSRVGetErrorString(eError)));

		goto e0;
	}

	/* Create VM manager para-virtualization connection */
	eError = VMMCreatePvzConnection(psDevConfig, (VMM_PVZ_CONNECTION **)&psPVRSRVData->hPvzConnection);
	if (eError != PVRSRV_OK)
	{
		OSLockDestroy(psPVRSRVData->hPvzConnectionLock);
		psPVRSRVData->hPvzConnectionLock = NULL;

		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Unable to create PVZ connection (%s)",
				__func__,
				PVRSRVGetErrorString(eError)));

		goto e0;
	}

	/* A native driver running (in a VM) exits here */
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_NATIVE, eError);

	/* Perform PVZ connection validation */
	psVmmPvz = SysVzPvzConnectionAcquire();
	if (psVmmPvz == NULL)
	{
		OSLockDestroy(psPVRSRVData->hPvzConnectionLock);
		psPVRSRVData->hPvzConnectionLock = NULL;

		PVR_DPF((PVR_DBG_ERROR,
				"%s: %s PVZ config: Unable to acquire PVZ connection",
				__func__,
				PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST) ? "Guest" : "Host"));

		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e0;
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
	else if (!PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST)           &&
			 (psVmmPvz->sServerFuncTab.pfnMapDevPhysHeap     == NULL ||
			  psVmmPvz->sServerFuncTab.pfnUnmapDevPhysHeap   == NULL))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Host PVZ config: Invalid guest function table setup",
				__func__));

		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e1;
	}

	/* Log which PVZ setup type is being used by driver */
#if defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
	/*
	 *  Static PVZ bootstrap setup
	 *
	 *  This setup uses carve-out memory, has no hypercall mechanism & does not support
	 *  out-of-order initialisation of host/guest VMs/drivers. The host driver has all
	 *  the information needed to initialize all OSIDs firmware state when it's loaded
	 *  and its PVZ layer must mark all guest OSIDs as being online as part of its PVZ
	 *  initialisation. Having no out-of-order initialisation support, the guest driver
	 *  can only submit a workload to the device after the host driver has completely
	 *  initialized the firmware, the VZ hypervisor/VM setup must guarantee this.
	 */
	PVR_LOG(("Using static PVZ bootstrap setup"));
#else
	/*
	 *  Dynamic PVZ bootstrap setup
	 *
	 *  This setup uses guest memory, has PVZ hypercall mechanism & supports out-of-order
	 *  initialisation of host/guest VMs/drivers. The host driver initializes only its
	 *  own OSID-0 firmware state when its loaded and each guest driver will use its PVZ
	 *  interface to hypercall to the host driver to both synchronise its initialisation
	 *  so it does not submit any workload to the firmware before the host driver has
	 *  had a chance to initialize the firmware and to also initialize its own OSID-x
	 *  firmware state.
	 */
	PVR_LOG(("Using dynamic PVZ bootstrap setup"));
#endif

e1:
	SysVzPvzConnectionRelease(psVmmPvz);
e0:
	return eError;
}

void SysVzPvzConnectionDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	VMMDestroyPvzConnection(psDevConfig, psPVRSRVData->hPvzConnection);
	psPVRSRVData->hPvzConnection = NULL;

	OSLockDestroy(psPVRSRVData->hPvzConnectionLock);
	psPVRSRVData->hPvzConnectionLock = NULL;
}

VMM_PVZ_CONNECTION* SysVzPvzConnectionAcquire(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVR_ASSERT(psPVRSRVData->hPvzConnection != NULL);
	return psPVRSRVData->hPvzConnection;
}

void SysVzPvzConnectionRelease(VMM_PVZ_CONNECTION *psParaVz)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	/* Nothing to do, sanity check the pointer passed back */
	PVR_ASSERT(psParaVz == psPVRSRVData->hPvzConnection);
}

/******************************************************************************
 End of file (vz_vmm_pvz.c)
******************************************************************************/
