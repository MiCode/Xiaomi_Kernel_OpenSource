/*************************************************************************/ /*!
@File
@Title          Common linux module setup
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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

#include <linux/module.h>

#include "pvr_debugfs.h"
#include "private_data.h"
#include "linkage.h"
#include "lists.h"
#include "power.h"
#include "env_connection.h"
#include "process_stats.h"
#include "module_common.h"
#include "pvrsrv.h"
#include "pvr_hwperf.h"
#include "km_apphint.h"
#ifdef CONFIG_DMA_SHARED_BUFFER
#undef CONFIG_DMA_SHARED_BUFFER
#endif
#include "pvr_drv.h"

#if defined(SUPPORT_NATIVE_FENCE_SYNC)
#include "pvr_sync.h"
#endif

#if defined(SUPPORT_BUFFER_SYNC)
#include "pvr_buffer_sync.h"
#endif

#if defined(SUPPORT_GPUTRACE_EVENTS)
#include "pvr_gputrace.h"
#endif

#if defined(SUPPORT_KERNEL_SRVINIT)
#include "srvinit.h"
#endif

#if defined(SUPPORT_PVRSRV_GPUVIRT)
#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
#include "vmm_pvz_server.h"
#if defined(PVRSRV_GPUVIRT_MULTIDRV_MODEL)
#include "vmm_pvz_mdm.h"
#endif
#endif
#endif

#if defined(PVRSRV_NEED_PVR_DPF) || defined(DEBUG)
#include <linux/moduleparam.h>
#endif /* defined(PVRSRV_NEED_PVR_DPF) || defined(DEBUG) */

#if defined(PVRSRV_NEED_PVR_DPF)
extern IMG_UINT32 gPVRDebugLevel;
module_param(gPVRDebugLevel, uint, 0644);
MODULE_PARM_DESC(gPVRDebugLevel,
				 "Sets the level of debug output (default 0x7)");
#endif /* defined(PVRSRV_NEED_PVR_DPF) */

#if defined(DEBUG)
extern IMG_UINT32 gPMRAllocFail;
module_param(gPMRAllocFail, uint, 0644);
MODULE_PARM_DESC(gPMRAllocFail, "When number of PMR allocs reaches"
				 " this value, it will fail (default value is 0 which"
				 "means that alloc function will behave normally).");
#endif /* defined(DEBUG) */

#if defined(SUPPORT_DISPLAY_CLASS)
/* Display class interface */
#include "kerneldisplay.h"
EXPORT_SYMBOL(DCRegisterDevice);
EXPORT_SYMBOL(DCUnregisterDevice);
EXPORT_SYMBOL(DCDisplayConfigurationRetired);
EXPORT_SYMBOL(DCDisplayHasPendingCommand);
EXPORT_SYMBOL(DCImportBufferAcquire);
EXPORT_SYMBOL(DCImportBufferRelease);

/* Physmem interface (required by LMA DC drivers) */
#include "physheap.h"
EXPORT_SYMBOL(PhysHeapAcquire);
EXPORT_SYMBOL(PhysHeapRelease);
EXPORT_SYMBOL(PhysHeapGetType);
EXPORT_SYMBOL(PhysHeapRegionGetCpuPAddr);
EXPORT_SYMBOL(PhysHeapRegionGetSize);
EXPORT_SYMBOL(PhysHeapCpuPAddrToDevPAddr);

EXPORT_SYMBOL(PVRSRVSystemInstallDeviceLISR);
EXPORT_SYMBOL(PVRSRVSystemUninstallDeviceLISR);
#endif

#include "mtk_mfgsys.h"

#if defined(MTK_DEBUG_PROC_PRINT)
#include "mtk_pp.h"
#endif


/* Host para-virtz call handlers  (required by guest drivers) */
#if defined(PVRSRV_GPUVIRT_MULTIDRV_MODEL)
#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
EXPORT_SYMBOL(PvzServerInstallDevLISR);
EXPORT_SYMBOL(PvzServerUninstallDevLISR);
EXPORT_SYMBOL(PvzServerCreateDevConfig);
EXPORT_SYMBOL(PvzServerDestroyDevConfig);
EXPORT_SYMBOL(PvzServerCreateDevConfig2);
EXPORT_SYMBOL(PvzServerDestroyDevConfig2);
EXPORT_SYMBOL(PvzServerCreateDevPhysHeaps);
EXPORT_SYMBOL(PvzServerDestroyDevPhysHeaps);
EXPORT_SYMBOL(PvzServerMapDevPhysHeap);
EXPORT_SYMBOL(PvzServerUnmapDevPhysHeap);
EXPORT_SYMBOL(PvzServerCreateDevPhysHeaps2);
EXPORT_SYMBOL(PvzServerDestroyDevPhysHeaps2);
#endif
#endif

#if !(defined(PVRSRV_GPUVIRT_GUESTDRV) && defined(PVRSRV_GPUVIRT_MULTIDRV_MODEL))
#include "pvr_notifier.h"

/*
 * Export some symbols that may be needed by other drivers
 *
 * When support for GPU virtualization is present and the multi-driver
 * model (multiple drivers in same OS kernel) is being used, then only
 * the host driver is a true device drivers (i.e. is registered with
 * the kernel to manage the physical device), the other guest drivers
 * are all modules.
 */
EXPORT_SYMBOL(PVRSRVCheckStatus);
EXPORT_SYMBOL(PVRSRVGetDriverStatus);
EXPORT_SYMBOL(PVRSRVGetErrorStringKM);

#include "rgxapi_km.h"
#if defined(SUPPORT_SHARED_SLC) && !defined(PVRSRV_GPUVIRT_GUESTDRV)
/* Guest drivers do not perform device management so RGXInitSLC is absent */
EXPORT_SYMBOL(RGXInitSLC);
#endif

EXPORT_SYMBOL(RGXHWPerfConnect);
EXPORT_SYMBOL(RGXHWPerfDisconnect);
EXPORT_SYMBOL(RGXHWPerfControl);
EXPORT_SYMBOL(RGXHWPerfConfigureAndEnableCounters);
EXPORT_SYMBOL(RGXHWPerfDisableCounters);
EXPORT_SYMBOL(RGXHWPerfAcquireData);
EXPORT_SYMBOL(RGXHWPerfReleaseData);
#endif

CONNECTION_DATA *LinuxConnectionFromFile(struct file *pFile)
{
	if (pFile)
	{
		struct drm_file *psDRMFile = pFile->private_data;

		return psDRMFile->driver_priv;
	}

	return NULL;
}

struct file *LinuxFileFromConnection(CONNECTION_DATA *psConnection)
{
	ENV_CONNECTION_DATA *psEnvConnection;

	psEnvConnection = PVRSRVConnectionPrivateData(psConnection);
	PVR_ASSERT(psEnvConnection != NULL);

	return psEnvConnection->psFile;
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDriverInit
@Description  Common one time driver initialisation
@Return       int           0 on success and a Linux error code otherwise
*/ /***************************************************************************/
int PVRSRVCommonDriverInit(void)
{
	PVRSRV_ERROR pvrerr;
	int error = 0;

#if defined(PDUMP)
	error = dbgdrv_init();
	if (error != 0)
	{
		return error;
	}
#endif

	error = PVRDebugFSInit();
	if (error != 0)
	{
		return error;
	}

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	if (PVRSRVStatsInitialise() != PVRSRV_OK)
	{
		return -ENOMEM;
	}
#endif

	if (PVROSFuncInit() != PVRSRV_OK)
	{
		return -ENOMEM;
	}

	LinuxBridgeInit();

	pvrerr = PVRSRVDriverInit();
	if (pvrerr != PVRSRV_OK)
	{
		if (pvrerr == PVRSRV_ERROR_PROBE_DEFER)
			return -EPROBE_DEFER;
		else
			return -ENODEV;
	}
    
    /* MTK MFG system entry */
	MTKMFGSystemInit();
   
#if defined(MTK_DEBUG_PROC_PRINT)
	MTKPP_Init();
#endif

	return 0;
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDriverDeinit
@Description  Common one time driver de-initialisation
@Return       void
*/ /***************************************************************************/
void PVRSRVCommonDriverDeinit(void)
{

#if defined(MTK_DEBUG_PROC_PRINT)
	MTKPP_Deinit();
#endif
   
	/* MTK MFG sytem cleanup */
	MTKMFGSystemDeInit();
    
	PVRSRVDriverDeInit();

	LinuxBridgeDeInit();

	PVROSFuncDeInit();

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	PVRSRVStatsDestroy();
#endif
	PVRDebugFSDeInit();

#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDeviceInit
@Description  Common device related initialisation.
@Input        psDeviceNode  The device node for which initialisation should be
                            performed
@Return       int           0 on success and a Linux error code otherwise
*/ /***************************************************************************/
int PVRSRVCommonDeviceInit(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	int error = 0;

	PVR_UNREFERENCED_PARAMETER(psDeviceNode);

#if defined(SUPPORT_NATIVE_FENCE_SYNC)
	{
		PVRSRV_ERROR eError = pvr_sync_init(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: unable to create sync (%d)",
					 __func__, eError));
			return -EBUSY;
		}
	}
#endif

#if defined(SUPPORT_BUFFER_SYNC)
	psDeviceNode->psBufferSyncContext =
		pvr_buffer_sync_context_create(psDeviceNode);
	if (IS_ERR(psDeviceNode->psBufferSyncContext))
	{
		error = PTR_ERR(psDeviceNode->psBufferSyncContext);
		psDeviceNode->psBufferSyncContext = NULL;

		PVR_DPF((PVR_DBG_ERROR,
				 "%s: unable to initialise buffer_sync support (%d)",
				 __func__, error));
		return error;
	}
#endif

	error = PVRDebugCreateDebugFSEntries();
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_WARNING,
			 "%s: failed to create default debugfs entries (%d)",
			 __func__, error));
	}

#if defined(SUPPORT_GPUTRACE_EVENTS)
	error = PVRGpuTraceInit(psDeviceNode);
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_WARNING,
			 "%s: failed to initialise PVR GPU Tracing (%d)",
			 __func__, error));
	}
#endif

#if defined(SUPPORT_KERNEL_SRVINIT)
	error = pvr_apphint_init();
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_WARNING,
			 "%s: failed AppHint setup(%d)",
			 __func__, error));
	}
#else
	error = PVRSRVHWperfCreateDebugFs();
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_WARNING,
			 "%s: failed to initialise HWPerf debugfs (%d)",
			  __func__, error));
	}
#endif

	return 0;
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDeviceDeinit
@Description  Common device related de-initialisation.
@Input        psDeviceNode  The device node for which de-initialisation should
                            be performed
@Return       void
*/ /***************************************************************************/
void PVRSRVCommonDeviceDeinit(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);

#if defined(SUPPORT_KERNEL_SRVINIT)
	pvr_apphint_deinit();
#else
	PVRSRVHWperfDestroyDebugFs();
#endif

#if defined(SUPPORT_GPUTRACE_EVENTS)
	PVRGpuTraceDeInit(psDeviceNode);
#endif

	PVRDebugRemoveDebugFSEntries();

#if defined(SUPPORT_BUFFER_SYNC)
	pvr_buffer_sync_context_destroy(psDeviceNode->psBufferSyncContext);
#endif

#if defined(SUPPORT_NATIVE_FENCE_SYNC)
	pvr_sync_deinit();
#endif
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDeviceShutdown
@Description  Common device shutdown.
@Input        psDeviceNode  The device node representing the device that should
                            be shutdown
@Return       void
*/ /***************************************************************************/

void PVRSRVCommonDeviceShutdown(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	/*
	 * Take the bridge mutex, and never release it, to stop processes trying to
	 * use the driver after it has been shutdown.
	 */
	OSAcquireBridgeLock();

	(void) PVRSRVSetDeviceSystemPowerState(psDeviceNode,
										   PVRSRV_SYS_POWER_STATE_OFF);
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDeviceSuspend
@Description  Common device suspend.
@Input        psDeviceNode  The device node representing the device that should
                            be suspended
@Return       int           0 on success and a Linux error code otherwise
*/ /***************************************************************************/
int PVRSRVCommonDeviceSuspend(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	/*
	 * OSSetDriverSuspended prevents processes from using the driver while it's
	 * suspended (this is needed for Android). Acquire the bridge lock first to
	 * ensure the driver isn't currently in use.
	 */
	OSAcquireBridgeLock();
	OSSetDriverSuspended();
	OSReleaseBridgeLock();

	if (PVRSRVSetDeviceSystemPowerState(psDeviceNode,
										PVRSRV_SYS_POWER_STATE_OFF) != PVRSRV_OK)
	{
		OSClearDriverSuspended();
		return -EINVAL;
	}

	return 0;
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDeviceResume
@Description  Common device resume.
@Input        psDeviceNode  The device node representing the device that should
                            be resumed
@Return       int           0 on success and a Linux error code otherwise
*/ /***************************************************************************/
int PVRSRVCommonDeviceResume(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	if (PVRSRVSetDeviceSystemPowerState(psDeviceNode,
										PVRSRV_SYS_POWER_STATE_ON) != PVRSRV_OK)
	{
		return -EINVAL;
	}

	OSClearDriverSuspended();

	/*
	 * Reprocess the device queues in case commands were blocked during
	 * suspend.
	 */
	if (psDeviceNode->eDevState == PVRSRV_DEVICE_STATE_ACTIVE)
	{
		PVRSRVCheckStatus(NULL);
	}

	return 0;
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDeviceOpen
@Description  Common device open.
@Input        psDeviceNode  The device node representing the device being
                            opened by a user mode process
@Input        psDRMFile     The DRM file data that backs the file handle
                            returned to the user mode process
@Return       int           0 on success and a Linux error code otherwise
*/ /***************************************************************************/
int PVRSRVCommonDeviceOpen(PVRSRV_DEVICE_NODE *psDeviceNode,
						   struct drm_file *psDRMFile)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	ENV_CONNECTION_PRIVATE_DATA sPrivData;
	void *pvConnectionData;
	PVRSRV_ERROR eError;
	int iErr = 0;

	OSAcquireBridgeLock();

	if (!psPVRSRVData)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: No device data", __func__));
		iErr = -ENODEV;
		goto e1;
	}

#if defined(SUPPORT_KERNEL_SRVINIT)
	if (psDeviceNode->eDevState == PVRSRV_DEVICE_STATE_INIT)
	{
		eError = PVRSRVDeviceInitialise(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to initialise device (%s)",
					 __func__, PVRSRVGetErrorStringKM(eError)));
			iErr = -ENODEV;
			goto e1;
		}
	}
#endif

	sPrivData.psDevNode = psDeviceNode;
	sPrivData.psFile = psDRMFile->filp;

	/*
	 * Here we pass the file pointer which will passed through to our
	 * OSConnectionPrivateDataInit function where we can save it so
	 * we can back reference the file structure from it's connection
	 */
	eError = PVRSRVConnectionConnect(&pvConnectionData, (void *) &sPrivData);
	if (eError != PVRSRV_OK)
	{
		iErr = -ENOMEM;
		goto e1;
	}

	psDRMFile->driver_priv = pvConnectionData;
	OSReleaseBridgeLock();

out:
	return iErr;
e1:
	OSReleaseBridgeLock();
	goto out;
}

/**************************************************************************/ /*!
@Function     PVRSRVCommonDeviceRelease
@Description  Common device release.
@Input        psDeviceNode  The device node for the device that the given file
                            represents
@Input        psDRMFile     The DRM file data that's being released
@Return       void
*/ /***************************************************************************/
void PVRSRVCommonDeviceRelease(PVRSRV_DEVICE_NODE *psDeviceNode,
							   struct drm_file *psDRMFile)
{
	void *pvConnectionData;

	PVR_UNREFERENCED_PARAMETER(psDeviceNode);

	OSAcquireBridgeLock();

	pvConnectionData = psDRMFile->driver_priv;
	if (pvConnectionData)
	{
		PVRSRVConnectionDisconnect(pvConnectionData);
		psDRMFile->driver_priv = NULL;
	}

	OSReleaseBridgeLock();
}
