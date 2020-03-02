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
#include "pvrsrv_error.h"
#include "rgxheapconfig.h"

#include "vmm_impl.h"
#include "vmm_pvz_server.h"

static PVRSRV_ERROR
StubVMMCreateDevConfig(IMG_UINT32 ui32FuncID,
					   IMG_UINT32 ui32DevID,
					   IMG_UINT32 *pui32IRQ,
					   IMG_UINT32 *pui32RegsSize,
					   IMG_UINT64 *pui64RegsCpuPBase)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	PVR_UNREFERENCED_PARAMETER(pui32IRQ);
	PVR_UNREFERENCED_PARAMETER(pui32RegsSize);
	PVR_UNREFERENCED_PARAMETER(pui64RegsCpuPBase);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
StubVMMDestroyDevConfig(IMG_UINT32 ui32FuncID,
						IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
StubVMMCreateDevPhysHeaps(IMG_UINT32 ui32FuncID,
						  IMG_UINT32 ui32DevID,
						  IMG_UINT32 *peType,
						  IMG_UINT64 *pui64FwPhysHeapSize,
						  IMG_UINT64 *pui64FwPhysHeapAddr,
						  IMG_UINT64 *pui64GpuPhysHeapSize,
						  IMG_UINT64 *pui64GpuPhysHeapAddr)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	PVR_UNREFERENCED_PARAMETER(peType);
	PVR_UNREFERENCED_PARAMETER(pui64FwPhysHeapSize);
	PVR_UNREFERENCED_PARAMETER(pui64FwPhysHeapAddr);
	PVR_UNREFERENCED_PARAMETER(pui64GpuPhysHeapSize);
	PVR_UNREFERENCED_PARAMETER(pui64GpuPhysHeapAddr);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
StubVMMDestroyDevPhysHeaps(IMG_UINT32 ui32FuncID,
						   IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

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
StubVMMGetDevPhysHeapOrigin(PVRSRV_DEVICE_CONFIG *psDevConfig,
							PVRSRV_DEVICE_PHYS_HEAP eHeapType,
							PVRSRV_DEVICE_PHYS_HEAP_ORIGIN *peOrigin)
{
	*peOrigin = PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST;
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(eHeapType);
	return PVRSRV_OK;
}

static PVRSRV_ERROR
StubVMMGetDevPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
							  PVRSRV_DEVICE_PHYS_HEAP eHeapType,
							  IMG_UINT64 *pui64Size,
							  IMG_UINT64 *pui64Addr)
{
	*pui64Size = 0;
	*pui64Addr = 0;
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(eHeapType);
	return PVRSRV_OK;
}

static VMM_PVZ_CONNECTION gsStubVmmPvz =
{
	.sHostFuncTab = {
		/* pfnCreateDevConfig */
		&StubVMMCreateDevConfig,

		/* pfnDestroyDevConfig */
		&StubVMMDestroyDevConfig,

		/* pfnCreateDevPhysHeaps */
		&StubVMMCreateDevPhysHeaps,

		/* pfnDestroyDevPhysHeaps */
		&StubVMMDestroyDevPhysHeaps,

		/* pfnMapDevPhysHeap */
		&StubVMMMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&StubVMMUnmapDevPhysHeap
	},

	.sGuestFuncTab = {
		/* pfnCreateDevConfig */
		&PvzServerCreateDevConfig,

		/* pfnDestroyDevConfig */
		&PvzServerDestroyDevConfig,

		/* pfnCreateDevPhysHeaps */
		&PvzServerCreateDevPhysHeaps,

		/* pfnDestroyDevPhysHeaps */
		&PvzServerDestroyDevPhysHeaps,

		/* pfnMapDevPhysHeap */
		&PvzServerMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&PvzServerUnmapDevPhysHeap
	},

	.sConfigFuncTab = {
		/* pfnGetDevPhysHeapOrigin */
		&StubVMMGetDevPhysHeapOrigin,

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

PVRSRV_ERROR VMMCreatePvzConnection(VMM_PVZ_CONNECTION **psPvzConnection)
{
	PVR_LOGR_IF_FALSE((NULL != psPvzConnection), "VMMCreatePvzConnection", PVRSRV_ERROR_INVALID_PARAMS);
	*psPvzConnection = &gsStubVmmPvz;
	PVR_DPF((PVR_DBG_ERROR, "Using a stub VM manager type, no runtime VZ support"));
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

void VMMDestroyPvzConnection(VMM_PVZ_CONNECTION *psPvzConnection)
{
	PVR_LOG_IF_FALSE((NULL != psPvzConnection), "VMMDestroyPvzConnection");
}

/******************************************************************************
 End of file (vmm_type_stub.c)
******************************************************************************/
