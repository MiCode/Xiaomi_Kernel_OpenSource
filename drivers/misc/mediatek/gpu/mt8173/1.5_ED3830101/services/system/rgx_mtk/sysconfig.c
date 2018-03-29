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
#include "sysconfig.h"
#include "physheap.h"
#if defined(SUPPORT_ION)
#include "ion_support.h"
#endif
#include "mtk_mfgsys.h"

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>


#define RGX_CR_ISP_GRIDOFFSET                             (0x0FA0U)

static RGX_TIMING_INFORMATION	gsRGXTimingInfo;
static RGX_DATA					gsRGXData;
static PVRSRV_DEVICE_CONFIG		gsDevices[1];
static PVRSRV_SYSTEM_CONFIG		gsSysConfig;

static PHYS_HEAP_FUNCTIONS	gsPhysHeapFuncs;
static PHYS_HEAP_CONFIG		gsPhysHeapConfig;

/*
	CPU to Device physcial address translation
*/
static
IMG_VOID UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
									IMG_UINT32 ui32NumOfAddr,
									IMG_DEV_PHYADDR *psDevPAddr,
									IMG_CPU_PHYADDR *psCpuPAddr)

{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psDevPAddr[0].uiAddr = psCpuPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1) {
		IMG_UINT32 ui32Idx;

		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
			psDevPAddr[ui32Idx].uiAddr = psCpuPAddr[ui32Idx].uiAddr;
	}
}

/*
	Device to CPU physcial address translation
*/
static
IMG_VOID UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
									IMG_UINT32 ui32NumOfAddr,
									IMG_CPU_PHYADDR *psCpuPAddr,
									IMG_DEV_PHYADDR *psDevPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psCpuPAddr[0].uiAddr = psDevPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1) {
		IMG_UINT32 ui32Idx;

		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
			psCpuPAddr[ui32Idx].uiAddr = psDevPAddr[ui32Idx].uiAddr;
	}
}

/*
	SysCreateConfigData
*/
PVRSRV_ERROR SysCreateConfigData(PVRSRV_SYSTEM_CONFIG **ppsSysConfig, void *hDevice)
{
	PVR_UNREFERENCED_PARAMETER(hDevice);

	/* Setup information about physaical memory heap(s) we have */
	gsPhysHeapFuncs.pfnCpuPAddrToDevPAddr = UMAPhysHeapCpuPAddrToDevPAddr;
	gsPhysHeapFuncs.pfnDevPAddrToCpuPAddr = UMAPhysHeapDevPAddrToCpuPAddr;

	gsPhysHeapConfig.ui32PhysHeapID = 0;
	gsPhysHeapConfig.pszPDumpMemspaceName = "SYSMEM";
	gsPhysHeapConfig.eType = PHYS_HEAP_TYPE_UMA;
	gsPhysHeapConfig.psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig.hPrivData = (IMG_HANDLE)&gsSysConfig;
	gsPhysHeapConfig.sStartAddr.uiAddr = 0;
	gsPhysHeapConfig.uiSize = 0;

	gsSysConfig.pasPhysHeaps = &gsPhysHeapConfig;
	gsSysConfig.ui32PhysHeapCount = sizeof(gsPhysHeapConfig) / sizeof(PHYS_HEAP_CONFIG);

	gsSysConfig.pui32BIFTilingHeapConfigs = gauiBIFTilingHeapXStrides;
	gsSysConfig.ui32BIFTilingHeapCount = IMG_ARR_NUM_ELEMS(gauiBIFTilingHeapXStrides);

	/* Setup RGX specific timing data */
	gsRGXTimingInfo.ui32CoreClockSpeed		= RGX_HW_CORE_CLOCK_SPEED;

#if defined(MTK_ENABLE_SWAPM)
	gsRGXTimingInfo.bEnableActivePM			= IMG_TRUE;
	gsRGXTimingInfo.ui32ActivePMLatencyms	= SYS_RGX_ACTIVE_POWER_LATENCY_MS;
#else
	gsRGXTimingInfo.bEnableActivePM			= IMG_FALSE;
#endif

#if defined(MTK_ENABLE_HWAPM)
	gsRGXTimingInfo.bEnableRDPowIsland		= IMG_TRUE;
#else
	gsRGXTimingInfo.bEnableRDPowIsland		= IMG_FALSE;
#endif

	/* Setup RGX specific data */
	gsRGXData.psRGXTimingInfo = &gsRGXTimingInfo;

	/* Setup RGX device */
	gsDevices[0].eDeviceType            = PVRSRV_DEVICE_TYPE_RGX;
	gsDevices[0].pszName                = "RGX";

	/* Device setup information */
	/* MTK: using device tree */
	{
		struct resource *irq_res;
		struct resource *reg_res;

		irq_res = platform_get_resource(gpsPVRLDMDev, IORESOURCE_IRQ, 0);

		if (irq_res) {
			gsDevices[0].ui32IRQ			= irq_res->start;
			gsDevices[0].bIRQIsShared		= IMG_FALSE;
			gsDevices[0].eIRQActiveLevel	= PVRSRV_DEVICE_IRQ_ACTIVE_LOW;

			PVR_DPF((PVR_DBG_MESSAGE, "irq_res = 0x%x", (int)irq_res->start));
		} else {
			PVR_DPF((PVR_DBG_ERROR, "irq_res = NULL!"));
err_out:
			return PVRSRV_ERROR_INIT_FAILURE;
		}

		reg_res = platform_get_resource(gpsPVRLDMDev, IORESOURCE_MEM, 0);

		if (reg_res) {
			gsDevices[0].sRegsCpuPBase.uiAddr	= reg_res->start;
			gsDevices[0].ui32RegsSize			= resource_size(reg_res);

			PVR_DPF((PVR_DBG_MESSAGE, "reg_res = 0x%x, 0x%x",
				(int)reg_res->start, (int)resource_size(reg_res)));
		} else {
			PVR_DPF((PVR_DBG_ERROR, "reg_res = NULL!"));
			goto err_out;
		}
	}

	/* power management on HW system */
#if 1 //defined(MTK_ENABLE_SWAPM)
	gsDevices[0].pfnPrePowerState       = MTKDevPrePowerState;
	gsDevices[0].pfnPostPowerState      = MTKDevPostPowerState;
#else
	gsDevices[0].pfnPrePowerState       = IMG_NULL;
	gsDevices[0].pfnPostPowerState      = IMG_NULL;
#endif

	/* clock frequency  */
	gsDevices[0].pfnClockFreqGet        = IMG_NULL;

	/*  interrupt handled  */
	gsDevices[0].pfnInterruptHandled    = IMG_NULL;

	gsDevices[0].hDevData               = &gsRGXData;

	/* Setup system config */
	gsSysConfig.pszSystemName = RGX_HW_SYSTEM_NAME;
	gsSysConfig.uiDeviceCount = sizeof(gsDevices)/sizeof(gsDevices[0]);
	gsSysConfig.pasDevices = &gsDevices[0];

	/* power management on HW system */
#if 1 //defined(MTK_ENABLE_SWAPM)
	gsSysConfig.pfnSysPrePowerState = MTKSystemPrePowerState;
	gsSysConfig.pfnSysPostPowerState = MTKSystemPostPowerState;
#else
	gsSysConfig.pfnSysPrePowerState = IMG_NULL;
	gsSysConfig.pfnSysPostPowerState = IMG_NULL;
#endif

	/* cache snooping */
	gsSysConfig.eCacheSnoopingMode = 0;

	gsSysConfig.uiSysFlags = 0;

	/* Setup other system specific stuff */
#if defined(SUPPORT_ION)
	IonInit(NULL);
#endif

	*ppsSysConfig = &gsSysConfig;

	return PVRSRV_OK;
}

/*
	SysDestroyConfigData
*/
IMG_VOID SysDestroyConfigData(PVRSRV_SYSTEM_CONFIG *psSysConfig)
{
	PVR_UNREFERENCED_PARAMETER(psSysConfig);

#if defined(SUPPORT_ION)
	IonDeinit();
#endif
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_SYSTEM_CONFIG *psSysConfig, DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf)
{
	PVR_UNREFERENCED_PARAMETER(psSysConfig);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	return PVRSRV_OK;
}

/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
