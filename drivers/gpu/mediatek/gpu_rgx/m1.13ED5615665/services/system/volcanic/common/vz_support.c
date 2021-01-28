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
#include "img_defs.h"
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

PVRSRV_ERROR SysVzDevInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	/* This is always called (possibly down into VM layer if any) to
	   allow the runtime to perform device and/or pvz connection
	   initialization specific to VM/native environment layer */
	eError = SysVzPvzConnectionInit(psDevConfig);
	PVR_LOG_RETURN_IF_ERROR(eError, "SysVzPvzConnectionInit");

	/* A native driver running (in a VM) exits here */
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_NATIVE, eError);

	psPVRSRVData->abVmOnline[RGXFW_HOST_OS] = IMG_TRUE;

	/* Perform general device physheap initialisation */
	eError = SysVzInitDevPhysHeaps(psDevConfig);
	PVR_LOG_RETURN_IF_ERROR(eError, "SysVzInitDevPhysHeaps");

	return eError;
}

PVRSRV_ERROR SysVzDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_NATIVE, PVRSRV_OK);

	SysVzDeInitDevPhysHeaps(psDevConfig);
	SysVzPvzConnectionDeInit(psDevConfig);

	return PVRSRV_OK;
}

/******************************************************************************
 End of file (vz_support.c)
******************************************************************************/
