/*************************************************************************/ /*!
@File           vz_physheap_tc.c
@Title          System virtualization physheap configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System virtualization physical heap configuration
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
#if defined(SUPPORT_RGX)
#include "rgxinit.h"
#include "rgxdevice.h"
#endif
#include "pvrsrv_device.h"
#include "rgxfwutils.h"

#include "dma_support.h"
#include "vz_support.h"
#include "vz_vmm_pvz.h"
#include "vz_physheap.h"

/* Valid values for the TC_MEMORY_CONFIG configuration option */
#define TC_MEMORY_LOCAL			(1)
#define TC_MEMORY_HOST			(2)
#define TC_MEMORY_HYBRID		(3)

/* Values for PHYS_HEAP_CONFIG identification */
#define PHYS_HEAP_ID_LMA		(0)
#define PHYS_HEAP_ID_DISPLAY	(1)
#define PHYS_HEAP_ID_UMA		(2)
#define PHYS_HEAP_ID_VZ			(3)

static PVRSRV_ERROR
SysVzCreateDisplayPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
#if defined(SUPPORT_DISPLAY_CLASS)
	IMG_UINT32 ui32Idx;
	PHYS_HEAP_CONFIG *psDisplayPhysHeapConfig = NULL;

	/* First, we locate the display physical heap configuration via ID */
	for (ui32Idx = 0; ui32Idx < psDevConfig->ui32PhysHeapCount; ui32Idx++)
	{
		if (psDevConfig->pasPhysHeaps[ui32Idx].ui32PhysHeapID == PHYS_HEAP_ID_DISPLAY)
		{
			psDisplayPhysHeapConfig = &psDevConfig->pasPhysHeaps[ui32Idx];
			PVR_ASSERT(!psDisplayPhysHeapConfig->bDynAlloc);
			break;
		}
	}

	if (psDisplayPhysHeapConfig == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: No display physical heap present\n", __func__));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_LOG(("Vz: Display memory region [cc=0]: sysbase 0x%08llx base 0x%08llx size 0x%08llx",
			(IMG_UINT64)psDisplayPhysHeapConfig->pasRegions[0].sStartAddr.uiAddr,
			psDisplayPhysHeapConfig->pasRegions[0].sCardBase.uiAddr,
			psDisplayPhysHeapConfig->pasRegions[0].uiSize));
#endif
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	return PVRSRV_OK;
}

static void
SysVzDestroyDisplayPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
}

static PVRSRV_ERROR
SysVzCreateGpuFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	IMG_UINT32 ui32Idx;
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_PHYS_HEAP eHeapType;
	PHYS_HEAP_CONFIG *psPhysHeapConfig;
	PHYS_HEAP_CONFIG *psGpuPhysHeapConfig;
#if defined(SUPPORT_RGX)
	PVRSRV_DEVICE_FABRIC_TYPE eDevFabricType;
	PVRSRV_DEVICE_SNOOP_MODE eCacheSnoopingMode;

	/* Need to know which coherency setting is currently enabled */
	eError = RGXSystemGetFabricCoherency(psDevConfig->sRegsCpuPBase,
										 psDevConfig->ui32RegsSize,
										 &eDevFabricType,
										 &eCacheSnoopingMode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Unable to obtain the device fabric coherency type",
				 __func__));
		return eError;
	}
#else
	PVRSRV_DEVICE_SNOOP_MODE eCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;
#endif

	/* Look up the graphics physical heap configuration */
	psGpuPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL);
	PVR_ASSERT(psGpuPhysHeapConfig && psGpuPhysHeapConfig->pasRegions);

	/* Resize the various physical (non)snooping heap regions */
	for (eHeapType=0; eHeapType < PVRSRV_DEVICE_PHYS_HEAP_LAST; eHeapType++)
	{
		switch (eHeapType)
		{
			/* Only interested in these physheap */
			case PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL:
			case PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL:
				break;

			default:
				continue;
		}

		/* Look up the firmware/graphics physical heap configuration */
		psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, eHeapType);
		PVR_ASSERT(psPhysHeapConfig && psPhysHeapConfig->pasRegions);

		eError = SysVzGetPhysHeapAddrSize(psDevConfig,
										  eHeapType,
										  SysVzGetMemoryConfigPhysHeapType(),
										  &psPhysHeapConfig->pasRegions[0].sCardBase,
										  &psPhysHeapConfig->pasRegions[0].uiSize);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}
		else
		{
			if (psPhysHeapConfig->ui32NumOfRegions == 0)
			{
				psPhysHeapConfig->ui32NumOfRegions++;
			}

			/* SysDevInit is responsible for passing these values upwards */
			PVR_ASSERT(psPhysHeapConfig->pasRegions[0].sStartAddr.uiAddr);
			if (eCacheSnoopingMode != PVRSRV_DEVICE_SNOOP_NONE)
			{
				PVR_ASSERT(psPhysHeapConfig->ui32NumOfRegions >= 2);
				PVR_ASSERT(psPhysHeapConfig->pasRegions[1].sStartAddr.uiAddr);
			}
		}

		switch (eHeapType)
		{
			case PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL:
				PVR_ASSERT(psPhysHeapConfig->pasRegions[0].uiSize != 0);
				PVR_ASSERT(psPhysHeapConfig->pasRegions[0].sCardBase.uiAddr != 0);
#if defined(LMA) || (defined(TC_MEMORY_CONFIG) && (TC_MEMORY_CONFIG == TC_MEMORY_LOCAL))
				if (eCacheSnoopingMode != PVRSRV_DEVICE_SNOOP_NONE)
				{
					PVR_ASSERT(psPhysHeapConfig->pasRegions[1].uiSize != 0);
					PVR_ASSERT(psPhysHeapConfig->pasRegions[1].sCardBase.uiAddr != 0);

					/* Resize by scaling using respective region 0 sCardBase values */
					psPhysHeapConfig->pasRegions[1].sCardBase.uiAddr +=
									psPhysHeapConfig->pasRegions[0].sCardBase.uiAddr;
					psPhysHeapConfig->pasRegions[1].uiSize =
									psPhysHeapConfig->pasRegions[0].uiSize;
				}
#elif !defined(LMA) || (defined(TC_MEMORY_CONFIG) && (TC_MEMORY_CONFIG == TC_MEMORY_HOST))
				/* In this config all OSID point to their respective carve-out as both
				   device and processor view of memory is identical */
				psPhysHeapConfig->pasRegions[0].sStartAddr.uiAddr =
									psPhysHeapConfig->pasRegions[0].sCardBase.uiAddr;
#endif
				break;

			case PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL:
			default:
				PVR_ASSERT(psPhysHeapConfig->pasRegions[0].uiSize != 0);
#if defined(LMA) || (defined(TC_MEMORY_CONFIG) && (TC_MEMORY_CONFIG == TC_MEMORY_LOCAL))
				if (eCacheSnoopingMode != PVRSRV_DEVICE_SNOOP_NONE)
				{
					PVR_ASSERT(psPhysHeapConfig->pasRegions[1].sCardBase.uiAddr != 0);

					/* Resize the firmware snooping (i.e. cc=1) physheap region 1; start
					   by undoing the adjustments made earlier to graphics heap sCardBase
					   and assigning that to the firmware snooping physheap region 1. The
					   same layout for non-snooping region is used by snooping region */
					psPhysHeapConfig->pasRegions[1].sCardBase.uiAddr =
									psGpuPhysHeapConfig->pasRegions[1].sCardBase.uiAddr;
					psPhysHeapConfig->pasRegions[1].sCardBase.uiAddr -=
									psGpuPhysHeapConfig->pasRegions[0].sCardBase.uiAddr;

					/* Next resize by scaling using respective region 0 sCardBase values */
					psPhysHeapConfig->pasRegions[1].sCardBase.uiAddr +=
									psPhysHeapConfig->pasRegions[0].sCardBase.uiAddr;
					psPhysHeapConfig->pasRegions[1].uiSize =
									psPhysHeapConfig->pasRegions[0].uiSize;
				}
#elif !defined(LMA) || (defined(TC_MEMORY_CONFIG) && (TC_MEMORY_CONFIG == TC_MEMORY_HOST))
				psPhysHeapConfig->pasRegions[0].sStartAddr.uiAddr =
									psPhysHeapConfig->pasRegions[0].sCardBase.uiAddr;
				PVR_ASSERT(psPhysHeapConfig->pasRegions[0].sCardBase.uiAddr != 0);
#endif
				break;
		}

		for (ui32Idx = 0; ui32Idx < psPhysHeapConfig->ui32NumOfRegions; ui32Idx += 1)
		{
			if (! psPhysHeapConfig->pasRegions[ui32Idx].sStartAddr.uiAddr)
			{
				continue;
			}

			/* Log the fact that these values have been adjusted under virtualization */
			PVR_LOG(("VZ: Device %s memory region [cc=%d]: sysbase 0x%08llx base 0x%08llx size 0x%08llx",
					eHeapType == PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL ? "graphics" : "firmware",
					ui32Idx,
					(IMG_UINT64)psPhysHeapConfig->pasRegions[ui32Idx].sStartAddr.uiAddr,
					psPhysHeapConfig->pasRegions[ui32Idx].sCardBase.uiAddr,
					psPhysHeapConfig->pasRegions[ui32Idx].uiSize));
		}
	}

	return eError;
}

static void
SysVzDestroyGpuFwPhysHeap(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
}

PHYS_HEAP_TYPE SysVzGetMemoryConfigPhysHeapType(void)
{
#if !defined(LMA) || (defined(TC_MEMORY_CONFIG) && (TC_MEMORY_CONFIG == TC_MEMORY_HOST))
	return PHYS_HEAP_TYPE_UMA;
#else
	return PHYS_HEAP_TYPE_LMA;
#endif
}

PVRSRV_ERROR SysVzInitDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR eError;

	eError = SysVzCreateGpuFwPhysHeap(psDevConfig);
	if (eError != PVRSRV_OK)
	{
		goto e0;
	}

	eError = SysVzCreateDisplayPhysHeap(psDevConfig);
	if (eError != PVRSRV_OK)
	{
		goto e0;
	}

e0:
	return eError;
}

void SysVzDeInitDevPhysHeaps(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	SysVzDestroyDisplayPhysHeap(psDevConfig);
	SysVzDestroyGpuFwPhysHeap(psDevConfig);
}

/******************************************************************************
 End of file (vz_physheap_tc.c)
******************************************************************************/
