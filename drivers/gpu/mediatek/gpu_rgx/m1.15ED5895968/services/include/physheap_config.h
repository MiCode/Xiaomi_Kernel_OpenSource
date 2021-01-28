/*************************************************************************/ /*!
@File           physheap_config.h
@Title          Physical Heap Config API
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    PhysHeap configs are created in system layer and stored in
                device config.
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

#ifndef PHYSHEAP_CONFIG_H
#define PHYSHEAP_CONFIG_H

#include "img_types.h"
#include "pvrsrv_memallocflags.h"
#include "pvrsrv_memalloc_physheap.h"

typedef IMG_UINT32 PHYS_HEAP_USAGE_FLAGS;

#define PHYS_HEAP_USAGE_GPU_LOCAL      (1<<PVRSRV_PHYS_HEAP_GPU_LOCAL)
#define PHYS_HEAP_USAGE_CPU_LOCAL      (1<<PVRSRV_PHYS_HEAP_CPU_LOCAL)
#define PHYS_HEAP_USAGE_FW_MAIN        (1<<PVRSRV_PHYS_HEAP_FW_MAIN)
#define PHYS_HEAP_USAGE_FW_CONFIG      (1<<PVRSRV_PHYS_HEAP_FW_CONFIG)
#define PHYS_HEAP_USAGE_EXTERNAL       (1<<PVRSRV_PHYS_HEAP_EXTERNAL)
#define PHYS_HEAP_USAGE_GPU_PRIVATE    (1<<PVRSRV_PHYS_HEAP_GPU_PRIVATE)
#define PHYS_HEAP_USAGE_GPU_COHERENT   (1<<PVRSRV_PHYS_HEAP_GPU_COHERENT)
#define PHYS_HEAP_USAGE_GPU_SECURE     (1<<PVRSRV_PHYS_HEAP_GPU_SECURE)
#define PHYS_HEAP_USAGE_FW_CODE        (1<<PVRSRV_PHYS_HEAP_FW_CODE)
#define PHYS_HEAP_USAGE_FW_PRIV_DATA   (1<<PVRSRV_PHYS_HEAP_FW_PRIV_DATA)
#define PHYS_HEAP_USAGE_WRAP           (1<<30)
#define PHYS_HEAP_USAGE_DISPLAY        (1<<31)

typedef void (*CpuPAddrToDevPAddr)(IMG_HANDLE hPrivData,
                                   IMG_UINT32 ui32NumOfAddr,
                                   IMG_DEV_PHYADDR *psDevPAddr,
                                   IMG_CPU_PHYADDR *psCpuPAddr);

typedef void (*DevPAddrToCpuPAddr)(IMG_HANDLE hPrivData,
                                   IMG_UINT32 ui32NumOfAddr,
                                   IMG_CPU_PHYADDR *psCpuPAddr,
                                   IMG_DEV_PHYADDR *psDevPAddr);

typedef IMG_UINT32 (*GetRegionId)(IMG_HANDLE hPrivData,
                                  PVRSRV_MEMALLOCFLAGS_T uiAllocationFlags);

typedef struct _PHYS_HEAP_FUNCTIONS_
{
	/*! Translate CPU physical address to device physical address */
	CpuPAddrToDevPAddr	pfnCpuPAddrToDevPAddr;
	/*! Translate device physical address to CPU physical address */
	DevPAddrToCpuPAddr	pfnDevPAddrToCpuPAddr;
} PHYS_HEAP_FUNCTIONS;

typedef enum _PHYS_HEAP_TYPE_
{
	PHYS_HEAP_TYPE_UNKNOWN = 0,
	PHYS_HEAP_TYPE_UMA,
	PHYS_HEAP_TYPE_LMA,
	PHYS_HEAP_TYPE_DMA,
#if defined(SUPPORT_WRAP_EXTMEMOBJECT)
	PHYS_HEAP_TYPE_WRAP,
#endif
} PHYS_HEAP_TYPE;

typedef struct _PHYS_HEAP_CONFIG_
{
	IMG_UINT32				ui32PhysHeapID;
	PHYS_HEAP_TYPE			eType;
	IMG_CHAR				*pszPDumpMemspaceName;
	PHYS_HEAP_FUNCTIONS		*psMemFuncs;

	IMG_CPU_PHYADDR			sStartAddr;
	IMG_DEV_PHYADDR			sCardBase;
	IMG_UINT64				uiSize;

	IMG_HANDLE				hPrivData;

	PHYS_HEAP_USAGE_FLAGS   ui32UsageFlags;
} PHYS_HEAP_CONFIG;

#endif
