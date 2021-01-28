/*************************************************************************/ /*!
@File
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System Configuration functions
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

#include "pvrsrv_device.h"
#include "syscommon.h"
#include "vz_support.h"
#include "allocmem.h"
#include "sysinfo.h"
#include "sysconfig.h"
#include "physheap.h"
#if defined(SUPPORT_ION)
#include "ion_support.h"
#endif
#if defined(LINUX)
#include <linux/dma-mapping.h>
#endif
/*
 * In systems that support trusted device address protection, there are three
 * physical heaps from which pages should be allocated:
 * - one heap for normal allocations
 * - one heap for allocations holding META code memory
 * - one heap for allocations holding secured DRM data
 */

#define PHYS_HEAP_IDX_GENERAL     0
#define PHYS_HEAP_IDX_TDFWMEM     1
#define PHYS_HEAP_IDX_TDSECUREBUF 2
#define PHYS_HEAP_IDX_VIRTFW      3
#define PHYS_HEAP_IDX_FW_MEMORY   4

#if defined(SUPPORT_VALIDATION) && defined(PDUMP)
#include "validation_soc.h"
#endif

/*
	CPU to Device physical address translation
*/
static
void UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_DEV_PHYADDR *psDevPAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psDevPAddr[0].uiAddr = psCpuPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psDevPAddr[ui32Idx].uiAddr = psCpuPAddr[ui32Idx].uiAddr;
		}
	}
}

/*
	Device to CPU physical address translation
*/
static
void UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr,
								   IMG_DEV_PHYADDR *psDevPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psCpuPAddr[0].uiAddr = psDevPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psCpuPAddr[ui32Idx].uiAddr = psDevPAddr[ui32Idx].uiAddr;
		}
	}
}

static PHYS_HEAP_FUNCTIONS gsPhysHeapFuncs =
{
	/* pfnCpuPAddrToDevPAddr */
	UMAPhysHeapCpuPAddrToDevPAddr,
	/* pfnDevPAddrToCpuPAddr */
	UMAPhysHeapDevPAddrToCpuPAddr,
	/* pfnGetRegionId */
	NULL,
};

static PVRSRV_ERROR PhysHeapsCreate(PHYS_HEAP_CONFIG **ppasPhysHeapsOut,
									IMG_UINT32 *puiPhysHeapCountOut)
{
	PHYS_HEAP_CONFIG *pasPhysHeaps;
	static IMG_UINT32 uiHeapIDBase = 0;
	IMG_UINT32 ui32NextHeapID = 0;
	IMG_UINT32 uiHeapCount = 1;

#if defined(SUPPORT_TRUSTED_DEVICE)
	uiHeapCount += 1;
#elif defined(SUPPORT_DEDICATED_FW_MEMORY)
	uiHeapCount += 1;
#endif
	uiHeapCount += !PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE) ? 1:0;

	pasPhysHeaps = OSAllocZMem(sizeof(*pasPhysHeaps) * uiHeapCount);
	if (!pasPhysHeaps)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	pasPhysHeaps[ui32NextHeapID].ui32PhysHeapID = PHYS_HEAP_IDX_GENERAL;
	pasPhysHeaps[ui32NextHeapID].pszPDumpMemspaceName = "SYSMEM";
	pasPhysHeaps[ui32NextHeapID].eType = PHYS_HEAP_TYPE_UMA;
	pasPhysHeaps[ui32NextHeapID].psMemFuncs = &gsPhysHeapFuncs;
	ui32NextHeapID++;

#if defined(SUPPORT_TRUSTED_DEVICE)
	pasPhysHeaps[ui32NextHeapID].ui32PhysHeapID = PHYS_HEAP_IDX_TDFWMEM;
	pasPhysHeaps[ui32NextHeapID].pszPDumpMemspaceName = "TDFWMEM";
	pasPhysHeaps[ui32NextHeapID].eType = PHYS_HEAP_TYPE_UMA;
	pasPhysHeaps[ui32NextHeapID].psMemFuncs = &gsPhysHeapFuncs;
	ui32NextHeapID++;
#elif defined(SUPPORT_DEDICATED_FW_MEMORY)
	pasPhysHeaps[ui32NextHeapID].ui32PhysHeapID = PHYS_HEAP_IDX_FW_MEMORY;
	pasPhysHeaps[ui32NextHeapID].pszPDumpMemspaceName = "DEDICATEDFWMEM";
	pasPhysHeaps[ui32NextHeapID].eType = PHYS_HEAP_TYPE_UMA;
	pasPhysHeaps[ui32NextHeapID].psMemFuncs = &gsPhysHeapFuncs;
#endif

	if (! PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE))
	{
		pasPhysHeaps[ui32NextHeapID].ui32PhysHeapID = PHYS_HEAP_IDX_VIRTFW;
		pasPhysHeaps[ui32NextHeapID].pszPDumpMemspaceName = "SYSMEM";
		pasPhysHeaps[ui32NextHeapID].eType = PHYS_HEAP_TYPE_UMA;
		pasPhysHeaps[ui32NextHeapID].psMemFuncs = &gsPhysHeapFuncs;
		ui32NextHeapID++;
	}

	uiHeapIDBase += uiHeapCount;

	*ppasPhysHeapsOut = pasPhysHeaps;
	*puiPhysHeapCountOut = uiHeapCount;

	return PVRSRV_OK;
}

static void PhysHeapsDestroy(PHYS_HEAP_CONFIG *pasPhysHeaps)
{
	OSFreeMem(pasPhysHeaps);
}

PVRSRV_ERROR SysDevInit(void *pvOSDevice, PVRSRV_DEVICE_CONFIG **ppsDevConfig)
{
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	RGX_DATA *psRGXData;
	RGX_TIMING_INFORMATION *psRGXTimingInfo;
	PHYS_HEAP_CONFIG *pasPhysHeaps;
	IMG_UINT32 uiPhysHeapCount;
	PVRSRV_ERROR eError;

#if defined(LINUX)
	dma_set_mask(pvOSDevice, DMA_BIT_MASK(40));
#endif

	psDevConfig = OSAllocZMem(sizeof(*psDevConfig) +
							  sizeof(*psRGXData) +
							  sizeof(*psRGXTimingInfo));
	if (!psDevConfig)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psRGXData = (RGX_DATA *)((IMG_CHAR *)psDevConfig + sizeof(*psDevConfig));
	psRGXTimingInfo = (RGX_TIMING_INFORMATION *)((IMG_CHAR *)psRGXData + sizeof(*psRGXData));

	eError = PhysHeapsCreate(&pasPhysHeaps, &uiPhysHeapCount);
	if (eError)
	{
		goto ErrorFreeDevConfig;
	}

	/* Setup RGX specific timing data */
	psRGXTimingInfo->ui32CoreClockSpeed        = RGX_NOHW_CORE_CLOCK_SPEED;
	psRGXTimingInfo->bEnableActivePM           = IMG_FALSE;
	psRGXTimingInfo->bEnableRDPowIsland        = IMG_FALSE;
	psRGXTimingInfo->ui32ActivePMLatencyms     = SYS_RGX_ACTIVE_POWER_LATENCY_MS;

	/* Set up the RGX data */
	psRGXData->psRGXTimingInfo = psRGXTimingInfo;

#if defined(SUPPORT_TRUSTED_DEVICE)
	psRGXData->bHasTDFWMemPhysHeap = IMG_TRUE;
	psRGXData->uiTDFWMemPhysHeapID = PHYS_HEAP_IDX_TDFWMEM;
#elif defined(SUPPORT_DEDICATED_FW_MEMORY)
	psRGXData->bHasFWMemPhysHeap = IMG_TRUE;
	psRGXData->uiFWMemPhysHeapID = PHYS_HEAP_IDX_FW_MEMORY;
#endif

	/* Setup the device config */
	psDevConfig->pvOSDevice				= pvOSDevice;
	psDevConfig->pszName                = "nohw";
	psDevConfig->pszVersion             = NULL;

	/* Device setup information */
	psDevConfig->sRegsCpuPBase.uiAddr   = 0x00f00baa;
	psDevConfig->ui32RegsSize           = 0x4000;
	psDevConfig->ui32IRQ                = 0x00000bad;

	psDevConfig->pasPhysHeaps			= pasPhysHeaps;
	psDevConfig->ui32PhysHeapCount		= uiPhysHeapCount;

	psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL] = PHYS_HEAP_IDX_GENERAL;
	psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL] = PHYS_HEAP_IDX_GENERAL;
	psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL] = PHYS_HEAP_IDX_GENERAL;
	psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_EXTERNAL] = PHYS_HEAP_IDX_GENERAL;
	if (! PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE))
	{
		/* Virtualization support services needs to know which heap ID corresponds to FW */
		psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL] = PHYS_HEAP_IDX_VIRTFW;
	}

	/* No power management on no HW system */
	psDevConfig->pfnPrePowerState       = NULL;
	psDevConfig->pfnPostPowerState      = NULL;

	/* No clock frequency either */
	psDevConfig->pfnClockFreqGet        = NULL;

	psDevConfig->hDevData               = psRGXData;

	/* Setup other system specific stuff */
#if defined(SUPPORT_ION)
	IonInit(NULL);
#endif

	/* Pdump validation system registers */
#if defined(SUPPORT_VALIDATION) && defined(PDUMP)
	PVRSRVConfigureSysCtrl(NULL, PDUMP_FLAGS_CONTINUOUS);
#if defined(SUPPORT_SECURITY_VALIDATION)
	PVRSRVConfigureTrustedDevice(NULL, PDUMP_FLAGS_CONTINUOUS);
#endif
#endif

	psDevConfig->bHasFBCDCVersion31 = IMG_TRUE;

	*ppsDevConfig = psDevConfig;

	return PVRSRV_OK;

ErrorFreeDevConfig:
	OSFreeMem(psDevConfig);
	return eError;
}

void SysDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
#if defined(SUPPORT_ION)
	IonDeinit();
#endif

	PhysHeapsDestroy(psDevConfig->pasPhysHeaps);
	OSFreeMem(psDevConfig);
}

PVRSRV_ERROR SysInstallDeviceLISR(IMG_HANDLE hSysData,
								  IMG_UINT32 ui32IRQ,
								  const IMG_CHAR *pszName,
								  PFN_LISR pfnLISR,
								  void *pvData,
								  IMG_HANDLE *phLISRData)
{
	PVR_UNREFERENCED_PARAMETER(hSysData);
	PVR_UNREFERENCED_PARAMETER(ui32IRQ);
	PVR_UNREFERENCED_PARAMETER(pszName);
	PVR_UNREFERENCED_PARAMETER(pfnLISR);
	PVR_UNREFERENCED_PARAMETER(pvData);
	PVR_UNREFERENCED_PARAMETER(phLISRData);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
	PVR_UNREFERENCED_PARAMETER(hLISRData);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_DEVICE_CONFIG *psDevConfig,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	PVR_UNREFERENCED_PARAMETER(pvDumpDebugFile);
	return PVRSRV_OK;
}

/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
