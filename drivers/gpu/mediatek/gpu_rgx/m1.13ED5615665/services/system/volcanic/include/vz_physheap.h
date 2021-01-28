/*************************************************************************/ /*!
@File           vz_physheap.h
@Title          System virtualization physheap support APIs
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides physheaps virtualization-specific APIs
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

#ifndef VZ_PHYSHEAP_H
#define VZ_PHYSHEAP_H

#include "pvrsrv.h"

/*!
*******************************************************************************
 @Function      SysVzGetPhysHeapAddrSize
 @Description   Return the address and size value of the specified device heap
 @Return        PVRSRV_OK on success. Otherwise, a PVRSRV error code
******************************************************************************/
PVRSRV_ERROR
SysVzGetPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
						 PVRSRV_DEVICE_PHYS_HEAP eHeap,
						 PHYS_HEAP_TYPE eType,
						 IMG_DEV_PHYADDR *psAddr,
						 IMG_UINT64 *pui64Size);

/*!
*******************************************************************************
 @Function      SysVzSetPhysHeapAddrSize
 @Description   Set physical heap configuration attributes
 @Return        PVRSRV_OK on success. Otherwise, a PVRSRV error code
******************************************************************************/
PVRSRV_ERROR
SysVzSetPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
						 PVRSRV_DEVICE_PHYS_HEAP eHeap,
						 PHYS_HEAP_TYPE eType,
						 IMG_DEV_PHYADDR sAddr,
						 IMG_UINT64 ui64Size);

/*!
*******************************************************************************
 @Function      SysVzRegisterPhysHeap
 @Description   Registers heap with virtualization services
 @Return        PVRSRV_OK on success. Otherwise, a PVRSRV error code
******************************************************************************/
PVRSRV_ERROR
SysVzRegisterPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
					  PVRSRV_DEVICE_PHYS_HEAP eHeap);

/*!
*******************************************************************************
 @Function      SysVzDeregisterPhysHeap
 @Description   Deregister heap from virtualization services
 @Return        void
******************************************************************************/
void
SysVzDeregisterPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig,
						PVRSRV_DEVICE_PHYS_HEAP eHeap);


/*!
*******************************************************************************
 @Function      SysVzGetPhysHeapConfig
 @Description   Looks-up device physical heap configuration
 @Return        PHYS_HEAP_CONFIG * on success. Otherwise, NULL
******************************************************************************/
PHYS_HEAP_CONFIG*
SysVzGetPhysHeapConfig(PVRSRV_DEVICE_CONFIG *psDevConfig,
					   PVRSRV_DEVICE_PHYS_HEAP eHeap);

/*!
*******************************************************************************
 @Function      SysVzGetMemoryConfigPhysHeapType
 @Description   Get the platform memory configuration physical heap type
 @Return        PHYS_HEAP_TYPE
******************************************************************************/
PHYS_HEAP_TYPE SysVzGetMemoryConfigPhysHeapType(void);

/*!
*******************************************************************************
 @Function      SysVzInitDevPhysHeaps
 @Description   Initialize device physical heap
 @Return        PVRSRV_OK on success. Otherwise, a PVRSRV error code
******************************************************************************/
PVRSRV_ERROR SysVzInitDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig);

/*!
*******************************************************************************
 @Function      SysVzDeInitDevPhysHeaps
 @Description   DeInitialize device physical heap
 @Return        void
******************************************************************************/
void SysVzDeInitDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig);

#if !defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
/*!
*******************************************************************************
 @Function      SysVzRegisterFwPhysHeap
 @Description   Maps VM relative physically contiguous memory into the firmware
                kernel memory context
 @Return        PVRSRV_OK on success. Otherwise, a PVRSRV error code
*****************************************************************************/
PVRSRV_ERROR SysVzRegisterFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig);

/*!
*******************************************************************************
 @Function      SysVzUnregisterFwPhysHeap
 @Description   Unmaps VM relative physically contiguous memory from the
                firmware kernel memory context
 @Return        PVRSRV_OK on success. Otherwise, a PVRSRV error code
******************************************************************************/
PVRSRV_ERROR SysVzUnregisterFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig);
#endif

/*!
*******************************************************************************
 @Function      SysVzPvzRegisterFwPhysHeap
 @Description   Maps guest VM relative physically contiguous memory into the
                firmware kernel memory context
 @Return        PVRSRV_OK on success. Otherwise, a PVRSRV error code
*****************************************************************************/
PVRSRV_ERROR
SysVzPvzRegisterFwPhysHeap(IMG_UINT32 ui32OSID,
						   IMG_UINT32 ui32DevID,
						   IMG_UINT64 ui64Size,
						   IMG_UINT64 ui64Addr);

/*!
*******************************************************************************
 @Function      SysVzUnregisterFwPhysHeap
 @Description   Unmaps guest VM relative physically contiguous memory from
                the firmware kernel memory context
 @Return        PVRSRV_OK on success. Otherwise, a PVRSRV error code
******************************************************************************/
PVRSRV_ERROR
SysVzPvzUnregisterFwPhysHeap(IMG_UINT32 ui32OSID, IMG_UINT32 ui32DevID);

#endif /* VZ_PHYSHEAP_H */

/******************************************************************************
 End of file (vz_physheap.h)
******************************************************************************/
