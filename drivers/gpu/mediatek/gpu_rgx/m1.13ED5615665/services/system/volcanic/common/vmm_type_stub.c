/*************************************************************************/ /*!
@File			vmm_type_stub.c
@Title          Stub VM manager type
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Sample stub (no-operation) VM manager implementation
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
#include "rgxheapconfig.h"

#include "vmm_impl.h"
#include "vmm_pvz_server.h"

static PVRSRV_ERROR
StubVMMMapDevPhysHeap(IMG_UINT32 ui32FuncID,
					  IMG_UINT32 ui32DevID,
					  IMG_UINT64 ui64Size,
					  IMG_UINT64 ui64Addr)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	PVR_UNREFERENCED_PARAMETER(ui64Size);
	PVR_UNREFERENCED_PARAMETER(ui64Addr);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
StubVMMUnmapDevPhysHeap(IMG_UINT32 ui32FuncID,
						IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
StubVMMGetDevPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
							  PVRSRV_DEVICE_PHYS_HEAP eHeapType,
							  IMG_UINT64 *pui64Size,
							  IMG_UINT64 *pui64Addr)
{
#if defined(LMA)
	IMG_UINT64 ui32FwHeapBase=0x0, ui32GpuHeapBase;
	IMG_UINT64 ui32FwHeapSize=RGX_FIRMWARE_RAW_HEAP_SIZE, ui32GpuHeapSize;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_GUEST, PVRSRV_ERROR_NOT_IMPLEMENTED);

	/* In this setup, all available LMA memory is allocated to host (less fw heap size), no guest support */
	ui32GpuHeapSize = psDevConfig->pasPhysHeaps[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL].pasRegions[0].uiSize;
	ui32GpuHeapSize = ui32GpuHeapSize - ui32FwHeapSize;
	ui32GpuHeapBase = ui32FwHeapSize;

	switch (eHeapType)
	{
		case PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL:
			*pui64Size = ui32FwHeapSize;
			*pui64Addr = ui32FwHeapBase;
			break;

		case PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL:
			*pui64Size = ui32GpuHeapSize;
			*pui64Addr = ui32GpuHeapBase;
			break;

		default:
			*pui64Size = 0;
			*pui64Addr = 0;
			return PVRSRV_ERROR_NOT_IMPLEMENTED;
			break;
	}
#else
	*pui64Size = 0;
	*pui64Addr = 0;
#endif
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(eHeapType);
	return PVRSRV_OK;
}

static VMM_PVZ_CONNECTION gsStubVmmPvz =
{
	.sClientFuncTab = {
		/* pfnMapDevPhysHeap */
		&StubVMMMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&StubVMMUnmapDevPhysHeap
	},

	.sServerFuncTab = {
		/* pfnMapDevPhysHeap */
		&PvzServerMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&PvzServerUnmapDevPhysHeap
	},

	.sConfigFuncTab = {
		/* pfnGetDevPhysHeapAddrSize */
		&StubVMMGetDevPhysHeapAddrSize
	},

	.sVmmFuncTab = {
		/* pfnOnVmOnline */
		&PvzServerOnVmOnline,

		/* pfnOnVmOffline */
		&PvzServerOnVmOffline,

		/* pfnVMMConfigure */
		&PvzServerVMMConfigure
	}
};

PVRSRV_ERROR VMMCreatePvzConnection(PVRSRV_DEVICE_CONFIG *psDevConfig, VMM_PVZ_CONNECTION **psPvzConnection)
{
	PVR_LOG_RETURN_IF_FALSE((NULL != psPvzConnection), "VMMCreatePvzConnection", PVRSRV_ERROR_INVALID_PARAMS);
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	*psPvzConnection = &gsStubVmmPvz;
	return PVRSRV_OK;
}

void VMMDestroyPvzConnection(PVRSRV_DEVICE_CONFIG *psDevConfig, VMM_PVZ_CONNECTION *psPvzConnection)
{
	PVR_LOG_IF_FALSE((NULL != psPvzConnection), "VMMDestroyPvzConnection");
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
}

/******************************************************************************
 End of file (vmm_type_stub.c)
******************************************************************************/
