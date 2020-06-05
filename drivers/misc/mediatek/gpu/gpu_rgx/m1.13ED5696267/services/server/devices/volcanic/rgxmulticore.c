/*************************************************************************/ /*!
@File           rgxmulticore.c
@Title          Functions related to multicore devices
@Codingstyle    IMG
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Kernel mode workload estimation functionality.
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

#include "rgxdevice.h"
#include "rgxdefs_km.h"
#include "pdump_km.h"
#include "rgxmulticore.h"
#include "pvr_debug.h"

/*
 * RGXGetMultiCoreInfo:
 * Read multicore HW registers and fill in data structure for clients.
 * Return not supported on cores without multicore.
 */
PVRSRV_ERROR RGXGetMultiCoreInfo(PVRSRV_DEVICE_NODE *psDeviceNode,
                                 IMG_UINT32 ui32CapsSize,
                                 IMG_UINT32 *pui32NumCores,
                                 IMG_UINT64 *pui64Caps)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
#if defined(PLACEHOLDER_UNTIL_IMPLEMENTATION)
	/* Waiting for finalisation of future hardware */
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	void *hPrivate = (void*)&psDevInfo->sLayerParams;

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, GPU_MULTICORE_SUPPORT))
	{
		IMG_UINT32 ui32MulticoreRegBankOffset = (1 << RGX_GET_FEATURE_VALUE(psDevInfo, XPU_MAX_REGBANKS_ADDR_WIDTH));
		IMG_UINT32 ui32MulticoreGPUReg = RGX_CR_MULTICORE_GPU;
		IMG_UINT32 ui32NumCores;
		IMG_UINT32 i;

		ui32NumCores = RGXReadReg32(hPrivate, RGX_CR_MULTICORE_SYSTEM);
#if !defined(NO_HARDWARE)
		PVR_LOG(("Multicore system has %u cores", ui32NumCores));
		/* check that the number of cores reported is in-bounds */
		if (ui32NumCores > (RGX_CR_MULTICORE_SYSTEM_MASKFULL >> RGX_CR_MULTICORE_SYSTEM_GPU_COUNT_SHIFT))
		{
			PVR_DPF((PVR_DBG_ERROR, "invalid return (%u) read from MULTICORE_SYSTEM", ui32NumCores));
			return PVRSRV_ERROR_DEVICE_REGISTER_FAILED;
		}
#else
		/* simulation: currently we support one primary and one secondary */
		ui32NumCores = 2;
#endif

		*pui32NumCores = ui32NumCores;
		/* CapsSize of zero is allowed to just return number of cores */
		if (ui32CapsSize > 0)
		{
#if !defined(NO_HARDWARE)
			PVR_LOG(("Configured for %u multicores", ui32NumCores));
#endif
			if (ui32CapsSize < ui32NumCores)
			{
				eError = PVRSRV_ERROR_BRIDGE_BUFFER_TOO_SMALL;
			}
			else
			{
				for (i = 0; i < ui32NumCores; ++i)
				{
					*pui64Caps = RGXReadReg64(hPrivate, ui32MulticoreGPUReg) & ~0xFFFFFFFF;
#if !defined(NO_HARDWARE)
					PVR_LOG(("Core %d has capabilities value 0x%x", i, (IMG_UINT32)(*pui64Caps) ));
#else
					/* emulation for what we think caps are */
					*pui64Caps = i | ((i == 0) ? (RGX_CR_MULTICORE_GPU_CAPABILITY_PRIMARY_EN
												| RGX_CR_MULTICORE_GPU_CAPABILITY_GEOMETRY_EN) : 0)
								   | RGX_CR_MULTICORE_GPU_CAPABILITY_COMPUTE_EN
								   | RGX_CR_MULTICORE_GPU_CAPABILITY_FRAGMENT_EN;
#endif

					++pui64Caps;
					ui32MulticoreGPUReg += ui32MulticoreRegBankOffset;
				}
			}
		}
	}
	else
#endif
	{
		/* MULTICORE not supported on this device */
		PVR_DPF((PVR_DBG_ERROR, "Multicore not supported on this device"));
		eError = PVRSRV_ERROR_NOT_SUPPORTED;
	}

	return eError;
}
