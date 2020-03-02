/*************************************************************************/ /*!
@File			vmm_pvz_server.c
@Title          VM manager server para-virtualization handlers
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header provides VMM server para-virtz handler APIs
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
#include "img_types.h"
#include "img_defs.h"
#include "pvrsrv_error.h"

#include "vz_vm.h"
#include "vmm_impl.h"
#include "vz_vmm_pvz.h"
#include "vz_support.h"
#include "vmm_pvz_server.h"
#include "vz_physheap.h"


static inline void
PvzServerLockAcquire(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	OSLockAcquire(psPVRSRVData->hPvzConnectionLock);
}

static inline void
PvzServerLockRelease(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	OSLockRelease(psPVRSRVData->hPvzConnectionLock);
}


/*
 * ===========================================================
 *  The following server para-virtualization (pvz) functions
 *  are exclusively called by the VM manager (hypervisor) on
 *  behalf of guests to complete guest pvz calls
 *  (guest -> vm manager -> host)
 * ===========================================================
 */

PVRSRV_ERROR
PvzServerCreateDevConfig(IMG_UINT32 ui32OSID,
						 IMG_UINT32 ui32FuncID,
						 IMG_UINT32 ui32DevID,
						 IMG_UINT32 *pui32IRQ,
						 IMG_UINT32 *pui32RegsSize,
						 IMG_UINT64 *pui64RegsCpuPBase)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(ui32FuncID == PVZ_BRIDGE_CREATEDEVICECONFIG);

	eError = SysVzIsVmOnline(ui32OSID);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	PvzServerLockAcquire();

	eError = SysVzPvzCreateDevConfig(ui32OSID,
									 ui32DevID,
									 pui32IRQ,
									 pui32RegsSize,
									 pui64RegsCpuPBase);

	PvzServerLockRelease();

	return eError;
}

PVRSRV_ERROR
PvzServerDestroyDevConfig(IMG_UINT32 ui32OSID,
						  IMG_UINT32 ui32FuncID,
						  IMG_UINT32 ui32DevID)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(ui32FuncID == PVZ_BRIDGE_DESTROYDEVICECONFIG);

	eError = SysVzIsVmOnline(ui32OSID);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	PvzServerLockAcquire();

	eError = SysVzPvzDestroyDevConfig(ui32OSID, ui32DevID);

	PvzServerLockRelease();

	return eError;
}

PVRSRV_ERROR
PvzServerCreateDevPhysHeaps(IMG_UINT32 ui32OSID,
							IMG_UINT32 ui32FuncID,
							IMG_UINT32  ui32DevID,
							IMG_UINT32 *peHeapType,
							IMG_UINT64 *pui64FwSize,
							IMG_UINT64 *pui64FwAddr,
							IMG_UINT64 *pui64GpuSize,
							IMG_UINT64 *pui64GpuAddr)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(ui32FuncID == PVZ_BRIDGE_CREATEDEVICEPHYSHEAPS);

	eError = SysVzIsVmOnline(ui32OSID);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	PvzServerLockAcquire();

	eError = SysVzPvzCreateDevPhysHeaps(ui32OSID,
										ui32DevID,
										peHeapType,
										pui64FwSize,
										pui64FwAddr,
										pui64GpuSize,
										pui64GpuAddr);

	PvzServerLockRelease();

	return eError;
}

PVRSRV_ERROR
PvzServerDestroyDevPhysHeaps(IMG_UINT32 ui32OSID,
							 IMG_UINT32 ui32FuncID,
							 IMG_UINT32 ui32DevID)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(ui32FuncID == PVZ_BRIDGE_DESTROYDEVICEPHYSHEAPS);

	eError = SysVzIsVmOnline(ui32OSID);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	PvzServerLockAcquire();

	eError = SysVzPvzDestroyDevPhysHeaps(ui32OSID, ui32DevID);

	PvzServerLockRelease();

	return eError;
}

PVRSRV_ERROR
PvzServerMapDevPhysHeap(IMG_UINT32 ui32OSID,
						IMG_UINT32 ui32FuncID,
						IMG_UINT32 ui32DevID,
						IMG_UINT64 ui64Size,
						IMG_UINT64 ui64PAddr)
{
	PVRSRV_ERROR eError;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	VMM_PVZ_CONNECTION *psVmmPvz = SysVzPvzConnectionAcquire();
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eOrigin = PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_LAST;

	PVR_ASSERT(ui32FuncID == PVZ_BRIDGE_MAPDEVICEPHYSHEAP);

	eError = SysVzIsVmOnline(ui32OSID);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	PvzServerLockAcquire();

	eError = psVmmPvz->sConfigFuncTab.pfnGetDevPhysHeapOrigin(psPVRSRVData->psDeviceNodeList->psDevConfig,
															  PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL,
															  &eOrigin);

	if (eOrigin != PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST)
	{
		/* Reject hypercall if called with an incompatible PVZ physheap origin
		   configuration specified on host; here the guest has been configured
		   with guest-origin but host has not, both must use the same origin */
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Host PVZ config: Does not match with Guest PVZ config\n"
				"=>: pfnGetDevPhysHeapOrigin() is not identical with guest\n"
				"=>: host and guest(s) must use the same FW physheap origin",
				__func__));
		eError = PVRSRV_ERROR_INVALID_PVZ_CONFIG;
		goto e0;
	}

	eError = SysVzPvzRegisterFwPhysHeap(ui32OSID,
										ui32DevID,
										ui64Size,
										ui64PAddr);

e0:
	PvzServerLockRelease();
	SysVzPvzConnectionRelease(psVmmPvz);

	return eError;
}

PVRSRV_ERROR
PvzServerUnmapDevPhysHeap(IMG_UINT32 ui32OSID,
						  IMG_UINT32 ui32FuncID,
						  IMG_UINT32 ui32DevID)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(ui32FuncID == PVZ_BRIDGE_UNMAPDEVICEPHYSHEAP);

	eError = SysVzIsVmOnline(ui32OSID);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	PvzServerLockAcquire();

	eError = SysVzPvzUnregisterFwPhysHeap(ui32OSID, ui32DevID);

	PvzServerLockRelease();

	return eError;
}


/*
 * ============================================================
 *  The following server para-virtualization (pvz) functions
 *  are exclusively called by the VM manager (hypervisor) to
 *  pass side band information to the host (vm manager -> host)
 * ============================================================
 */

PVRSRV_ERROR
PvzServerOnVmOnline(IMG_UINT32 ui32OSID, IMG_UINT32 ui32Priority)
{
	PVRSRV_ERROR eError;

	PvzServerLockAcquire();

	eError = SysVzPvzOnVmOnline(ui32OSID, ui32Priority);

	PvzServerLockRelease();

	return eError;
}

PVRSRV_ERROR
PvzServerOnVmOffline(IMG_UINT32 ui32OSID)
{
	PVRSRV_ERROR eError;

	PvzServerLockAcquire();

	eError = SysVzPvzOnVmOffline(ui32OSID);

	PvzServerLockRelease();

	return eError;
}

PVRSRV_ERROR
PvzServerVMMConfigure(VMM_CONF_PARAM eVMMParamType, IMG_UINT32 ui32ParamValue)
{
	PVRSRV_ERROR eError;

	PvzServerLockAcquire();

	eError = SysVzPvzVMMConfigure(eVMMParamType, ui32ParamValue);

	PvzServerLockRelease();

	return eError;

}

/******************************************************************************
 End of file (vmm_pvz_server.c)
******************************************************************************/
