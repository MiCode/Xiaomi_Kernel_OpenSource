/*************************************************************************/ /*!
@File
@Title          RGX initialisation header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX initialisation
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

#if !defined(__RGXINIT_H__)
#define __RGXINIT_H__

#include "connection_server.h"
#include "pvrsrv_error.h"
#include "img_types.h"
#include "device.h"
#include "rgxdevice.h"
#include "rgx_bridge.h"
#include "rgxfwload.h"


/*!
*******************************************************************************

 @Function	RGXInitDevPart2

 @Description

 Second part of server-side RGX initialisation

 @Input psDeviceNode - device node

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR RGXInitDevPart2 (PVRSRV_DEVICE_NODE	*psDeviceNode,
							  IMG_UINT32			ui32DeviceFlags,
							  IMG_UINT32			ui32HWPerfHostBufSizeKB,
							  IMG_UINT32			ui32HWPerfHostFilter,
							  RGX_ACTIVEPM_CONF		eActivePMConf);

PVRSRV_ERROR RGXInitAllocFWImgMem(PVRSRV_DEVICE_NODE   *psDeviceNode,
                                  IMG_DEVMEM_SIZE_T    ui32FWCodeLen,
                                  IMG_DEVMEM_SIZE_T    ui32FWDataLen,
                                  IMG_DEVMEM_SIZE_T    uiFWCorememCodeLen,
                                  IMG_DEVMEM_SIZE_T    uiFWCorememDataLen);


/*!
*******************************************************************************

 @Function	RGXInitFirmware

 @Description

 Server-side RGX firmware initialisation

 @Input psDeviceNode - device node

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR
RGXInitFirmware(PVRSRV_DEVICE_NODE       *psDeviceNode,
                IMG_BOOL                 bEnableSignatureChecks,
                IMG_UINT32               ui32SignatureChecksBufSize,
                IMG_UINT32               ui32HWPerfFWBufSizeKB,
                IMG_UINT64               ui64HWPerfFilter,
                IMG_UINT32               ui32RGXFWAlignChecksArrLength,
                IMG_UINT32               *pui32RGXFWAlignChecks,
                IMG_UINT32               ui32ConfigFlags,
                IMG_UINT32               ui32LogType,
                IMG_UINT32               ui32FilterFlags,
                IMG_UINT32               ui32JonesDisableMask,
                IMG_UINT32               ui32HWRDebugDumpLimit,
                IMG_UINT32               ui32HWPerfCountersDataSize,
                IMG_UINT32               *pui32TPUTrilinearFracMask,
                RGX_RD_POWER_ISLAND_CONF eRGXRDPowerIslandingConf,
                FW_PERF_CONF             eFirmwarePerf,
                IMG_UINT32               ui32ConfigFlagsExt);


/*!
*******************************************************************************

 @Function	RGXLoadAndGetFWData

 @Description

 Load FW and return pointer to FW data.

 @Input psDeviceNode - device node

 @Input ppsRGXFW - fw pointer

 @Return   void * - pointer to FW data

******************************************************************************/
const void *RGXLoadAndGetFWData(PVRSRV_DEVICE_NODE *psDeviceNode, struct RGXFW **ppsRGXFW);

#if defined(PDUMP)
/*!
*******************************************************************************

 @Function	RGXInitHWPerfCounters

 @Description

 Initialisation of the performance counters

 @Input psDeviceNode - device node

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR RGXInitHWPerfCounters(PVRSRV_DEVICE_NODE	*psDeviceNode);
#endif

/*!
*******************************************************************************

 @Function	RGXRegisterDevice

 @Description

 Registers the device with the system

 @Input:   psDeviceNode - device node
 @Output:  ppsDevInfo   - device info

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR RGXRegisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode,
                               PVRSRV_RGXDEV_INFO **ppsDevInfo);

/*!
*******************************************************************************

 @Function	RGXDevBVNCString

 @Description

 Returns the Device BVNC string. It will allocate and fill it first, if necessary.

 @Input:   psDevInfo - device info (must not be null)

 @Return   IMG_PCHAR - pointer to BVNC string

******************************************************************************/
IMG_PCHAR RGXDevBVNCString(PVRSRV_RGXDEV_INFO *psDevInfo);

/*!
*******************************************************************************

 @Function	DevDeInitRGX

 @Description

 Reset and deinitialise Chip

 @Input psDeviceNode - device info. structure

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR DevDeInitRGX(PVRSRV_DEVICE_NODE *psDeviceNode);


#if !defined(NO_HARDWARE)

void RGX_WaitForInterruptsTimeout(PVRSRV_RGXDEV_INFO *psDevInfo);

/*!
*******************************************************************************

 @Function     SORgxGpuUtilStatsRegister

 @Description  SO Interface function called from the OS layer implementation.
               Initialise data used to compute GPU utilisation statistics
               for a particular user (identified by the handle passed as
               argument). This function must be called only once for each
               different user/handle.

 @Input        phGpuUtilUser - Pointer to handle used to identify a user of
                               RGXGetGpuUtilStats

 @Return       PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR SORgxGpuUtilStatsRegister(IMG_HANDLE *phGpuUtilUser);


/*!
*******************************************************************************

 @Function     SORgxGpuUtilStatsUnregister

 @Description  SO Interface function called from the OS layer implementation.
               Free data previously used to compute GPU utilisation statistics
               for a particular user (identified by the handle passed as
               argument).

 @Input        hGpuUtilUser - Handle used to identify a user of
                              RGXGetGpuUtilStats

 @Return       PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR SORgxGpuUtilStatsUnregister(IMG_HANDLE hGpuUtilUser);
#endif /* !defined(NO_HARDWARE) */


/*!
*******************************************************************************

 @Function		RGXVirtPopulateLMASubArenas

 @Description	Populates the LMA arenas based on the min max values passed by
				the client during initialization. GPU Virtualisation Validation
				only.

 @Input			psDeviceNode	: Pointer to a device info structure.
				ui32NumElements	: Total number of min / max values passed by
								  the client
				pui32Elements	: The array containing all the min / max values
								  passed by the client, all bundled together

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR RGXVirtPopulateLMASubArenas(PVRSRV_DEVICE_NODE	* psDeviceNode,
                                         IMG_UINT32 aui32OSidMin[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS],
                                         IMG_UINT32 aui32OSidMax[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS],
                                         IMG_BOOL bEnableTrustedDeviceAceConfig);

/*!
 *******************************************************************************

 @Function      RGXInitCreateFWKernelMemoryContext

 @Description   Called to perform initialisation during firmware kernel context
                creation.

 @Input         psDeviceNode  device node
 ******************************************************************************/
PVRSRV_ERROR RGXInitCreateFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode);

/*!
 *******************************************************************************

 @Function      RGXDeInitDestroyFWKernelMemoryContext

 @Description   Called to perform deinitialisation during firmware kernel
                context destruction.

 @Input         psDeviceNode  device node
 ******************************************************************************/
void RGXDeInitDestroyFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode);

/*!
 *******************************************************************************

 @Function      RGXVzInitCreateFWKernelMemoryContext

 @Description   Called to perform additional initialisation during firmware
                kernel context creation.

 @Input         psDeviceNode  device node
 ******************************************************************************/
PVRSRV_ERROR RGXVzInitCreateFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode);

/*!
 *******************************************************************************

 @Function      RGXVzDeInitDestroyFWKernelMemoryContext

 @Description   Called to perform additional deinitialisation during firmware
                kernel context destruction.
 ******************************************************************************/
void RGXVzDeInitDestroyFWKernelMemoryContext(PVRSRV_DEVICE_NODE *psDeviceNode);

#endif /* __RGXINIT_H__ */
