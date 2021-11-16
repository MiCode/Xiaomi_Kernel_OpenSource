/*************************************************************************/ /*!
@File
@Title          core services functions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Main APIs for core services functions
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

#include "img_defs.h"
#include "rgxdebug.h"
#include "handle.h"
#include "connection_server.h"
#include "osconnection_server.h"
#include "pdump_km.h"
#include "ra.h"
#include "allocmem.h"
#include "pmr.h"
#include "pvrsrv.h"
#include "srvcore.h"
#include "services_km.h"
#include "pvrsrv_device.h"
#include "pvr_debug.h"
#include "pvr_notifier.h"
#include "sync.h"
#include "sync_server.h"
#include "sync_checkpoint.h"
#include "sync_fallback_server.h"
#include "sync_checkpoint_init.h"
#include "devicemem.h"
#include "cache_km.h"
#include "info_page.h"
#include "info_page_defs.h"
#include "pvrsrv_bridge_init.h"
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
#include "devicemem_server.h"
#endif /* !defined(PVRSRV_USE_BRIDGE_LOCK) */

#include "log2.h"

#include "lists.h"
#include "dllist.h"
#include "syscommon.h"
#include "sysvalidation.h"

#include "physmem_lma.h"
#include "physmem_osmem.h"
#include "physmem_hostmem.h"

#include "tlintern.h"
#include "htbserver.h"

#if defined(SUPPORT_RGX)
#include "rgxinit.h"
#include "rgxhwperf.h"
#include "rgxfwutils.h"
#endif

#include "mtk_pp.h"
#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
#include "ri_server.h"
#endif

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif

#if defined(SUPPORT_GPUVIRT_VALIDATION)
	#if !defined(GPUVIRT_SIZEOF_ARENA0)
		#define GPUVIRT_SIZEOF_ARENA0	64 * 1024 * 1024 //Giving 64 megs of LMA memory to arena 0 for firmware and other allocations
	#endif
#endif

#include "devicemem_history_server.h"

#if defined(PVR_DVFS)
#include "pvr_dvfs_device.h"
#endif

#if defined(SUPPORT_DISPLAY_CLASS)
#include "dc_server.h"
#endif

#include "rgx_options.h"
#include "srvinit.h"
#include "rgxutils.h"

#include "oskm_apphint.h"
#include "pvrsrv_apphint.h"

#include "rgx_bvnc_defs_km.h"

#include "pvrsrv_tlstreams.h"
#include "tlstream.h"

#if defined(SUPPORT_PHYSMEM_TEST) && !defined(INTEGRITY_OS) && !defined(__QNXNTO__)
#include "physmem_test.h"
#endif

#if defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP)
#define INFINITE_SLEEP_TIMEOUT 0ULL
#endif

/*! Wait 100ms before retrying deferred clean-up again */
#define CLEANUP_THREAD_WAIT_RETRY_TIMEOUT 100000ULL

/*! Wait 8hrs when no deferred clean-up required. Allows a poll several times
 * a day to check for any missed clean-up. */
#if defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP)
#define CLEANUP_THREAD_WAIT_SLEEP_TIMEOUT INFINITE_SLEEP_TIMEOUT
#else
#define CLEANUP_THREAD_WAIT_SLEEP_TIMEOUT 28800000000ULL
#endif

/*! When unloading try a few times to free everything remaining on the list */
#define CLEANUP_THREAD_UNLOAD_RETRY 4

#define PVRSRV_PROC_HANDLE_BASE_INIT 10

#define PVRSRV_TL_CTLR_STREAM_SIZE 4096

static PVRSRV_DATA	*gpsPVRSRVData;
static IMG_UINT32 g_ui32InitFlags;

/* mark which parts of Services were initialised */
#define		INIT_DATA_ENABLE_PDUMPINIT	0x1U

static IMG_UINT32 g_aui32DebugOrderTable[] = {
	DEBUG_REQUEST_SYS,
	DEBUG_REQUEST_APPHINT,
	DEBUG_REQUEST_HTB,
	DEBUG_REQUEST_DC,
	DEBUG_REQUEST_SYNCCHECKPOINT,
	DEBUG_REQUEST_SERVERSYNC,
	DEBUG_REQUEST_ANDROIDSYNC,
	DEBUG_REQUEST_FALLBACKSYNC,
	DEBUG_REQUEST_LINUXFENCE
};

static PVRSRV_ERROR _VzDeviceCreate(PVRSRV_DEVICE_NODE *psDeviceNode);
static void _VzDeviceDestroy(PVRSRV_DEVICE_NODE *psDeviceNode);
static PVRSRV_ERROR _VzConstructRAforFwHeap(RA_ARENA **ppsArena, IMG_CHAR *szName,
											IMG_UINT64 uBase, RA_LENGTH_T uSize);
static void _VzTearDownRAforFwHeap(RA_ARENA **ppsArena, IMG_UINT64 uBase);

/* Callback to dump info of cleanup thread in debug_dump */
static void CleanupThreadDumpInfo(DUMPDEBUG_PRINTF_FUNC* pfnDumpDebugPrintf,
                                  void *pvDumpDebugFile)
{
	PVRSRV_DATA *psPVRSRVData;
	psPVRSRVData = PVRSRVGetPVRSRVData();

	PVR_DUMPDEBUG_LOG("    Number of deferred cleanup items : %u",
			  OSAtomicRead(&psPVRSRVData->i32NumCleanupItems));
}

/* Add work to the cleanup thread work list.
 * The work item will be executed by the cleanup thread
 */
void PVRSRVCleanupThreadAddWork(PVRSRV_CLEANUP_THREAD_WORK *psData)
{
	PVRSRV_DATA *psPVRSRVData;
	PVRSRV_ERROR eError;

	psPVRSRVData = PVRSRVGetPVRSRVData();

	PVR_ASSERT(psData != NULL);
#if defined(PVRSRV_FORCE_UNLOAD_IF_BAD_STATE)
	if (psPVRSRVData->eServicesState != PVRSRV_SERVICES_STATE_OK || psPVRSRVData->bUnload)
#else
	if (psPVRSRVData->bUnload)
#endif
	{
		CLEANUP_THREAD_FN pfnFree = psData->pfnFree;

		PVR_DPF((PVR_DBG_MESSAGE, "Cleanup thread has already quit: doing work immediately"));

		eError = pfnFree(psData->pvData);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "Failed to free resource "
						"(callback " IMG_PFN_FMTSPEC "). "
						"Immediate free will not be retried.",
						pfnFree));
		}
	}
	else
	{
		/* add this work item to the list */
		OSLockAcquire(psPVRSRVData->hCleanupThreadWorkListLock);
		dllist_add_to_tail(&psPVRSRVData->sCleanupThreadWorkList, &psData->sNode);
		OSLockRelease(psPVRSRVData->hCleanupThreadWorkListLock);

		OSAtomicIncrement(&psPVRSRVData->i32NumCleanupItems);

		/* signal the cleanup thread to ensure this item gets processed */
		eError = OSEventObjectSignal(psPVRSRVData->hCleanupEventObject);
		PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");
	}
}

/* Pop an item from the head of the cleanup thread work list */
static INLINE DLLIST_NODE *_CleanupThreadWorkListPop(PVRSRV_DATA *psPVRSRVData)
{
	DLLIST_NODE *psNode;

	OSLockAcquire(psPVRSRVData->hCleanupThreadWorkListLock);
	psNode = dllist_get_next_node(&psPVRSRVData->sCleanupThreadWorkList);
	if (psNode != NULL)
	{
		dllist_remove_node(psNode);
	}
	OSLockRelease(psPVRSRVData->hCleanupThreadWorkListLock);

	return psNode;
}

/* Process the cleanup thread work list */
static IMG_BOOL _CleanupThreadProcessWorkList(PVRSRV_DATA *psPVRSRVData,
                                              IMG_BOOL *pbUseGlobalEO)
{
	DLLIST_NODE *psNodeIter, *psNodeLast;
	PVRSRV_ERROR eError;
	IMG_BOOL bNeedRetry = IMG_FALSE;

	/* any callback functions which return error will be
	 * moved to the back of the list, and additional items can be added
	 * to the list at any time so we ensure we only iterate from the
	 * head of the list to the current tail (since the tail may always
	 * be changing)
	 */

	OSLockAcquire(psPVRSRVData->hCleanupThreadWorkListLock);
	psNodeLast = psPVRSRVData->sCleanupThreadWorkList.psPrevNode;
	OSLockRelease(psPVRSRVData->hCleanupThreadWorkListLock);

	do
	{
		PVRSRV_CLEANUP_THREAD_WORK *psData;

		psNodeIter = _CleanupThreadWorkListPop(psPVRSRVData);

		if (psNodeIter != NULL)
		{
			CLEANUP_THREAD_FN pfnFree;

			psData = IMG_CONTAINER_OF(psNodeIter, PVRSRV_CLEANUP_THREAD_WORK, sNode);

			/* get the function pointer address here so we have access to it
			 * in order to report the error in case of failure, without having
			 * to depend on psData not having been freed
			 */
			pfnFree = psData->pfnFree;

			*pbUseGlobalEO = psData->bDependsOnHW;
			eError = pfnFree(psData->pvData);

			if (eError != PVRSRV_OK)
			{
				/* move to back of the list, if this item's
				 * retry count hasn't hit zero.
				 */
				if (CLEANUP_THREAD_IS_RETRY_TIMEOUT(psData))
				{
					if (CLEANUP_THREAD_RETRY_TIMEOUT_REACHED(psData))
					{
						bNeedRetry = IMG_TRUE;
					}
				}
				else
				{
					if (psData->ui32RetryCount-- > 0)
					{
						bNeedRetry = IMG_TRUE;
					}
				}

				if (bNeedRetry)
				{
					OSLockAcquire(psPVRSRVData->hCleanupThreadWorkListLock);
					dllist_add_to_tail(&psPVRSRVData->sCleanupThreadWorkList, psNodeIter);
					OSLockRelease(psPVRSRVData->hCleanupThreadWorkListLock);
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "Failed to free resource "
								"(callback " IMG_PFN_FMTSPEC "). "
								"Retry limit reached",
								pfnFree));
				}
			}
			else
			{
				OSAtomicDecrement(&psPVRSRVData->i32NumCleanupItems);
			}
		}
	} while ((psNodeIter != NULL) && (psNodeIter != psNodeLast));

	return bNeedRetry;
}

// #define CLEANUP_DPFL PVR_DBG_WARNING
#define CLEANUP_DPFL    PVR_DBG_MESSAGE

/* Create/initialise data required by the cleanup thread,
 * before the cleanup thread is started
 */
static PVRSRV_ERROR _CleanupThreadPrepare(PVRSRV_DATA *psPVRSRVData)
{
	PVRSRV_ERROR eError;

	/* Create the clean up event object */

	eError = OSEventObjectCreate("PVRSRV_CLEANUP_EVENTOBJECT", &gpsPVRSRVData->hCleanupEventObject);
	PVR_LOGG_IF_ERROR(eError, "OSEventObjectCreate", Exit);

	/* initialise the mutex and linked list required for the cleanup thread work list */

	eError = OSLockCreate(&psPVRSRVData->hCleanupThreadWorkListLock);
	PVR_LOGG_IF_ERROR(eError, "OSLockCreate", Exit);

	dllist_init(&psPVRSRVData->sCleanupThreadWorkList);

Exit:
	return eError;
}

static void CleanupThread(void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = pvData;
	IMG_BOOL     bRetryWorkList = IMG_FALSE;
	IMG_HANDLE	 hGlobalEvent;
	IMG_HANDLE	 hOSEvent;
	PVRSRV_ERROR eRc;
	IMG_BOOL bUseGlobalEO = IMG_FALSE;
	IMG_UINT32 uiUnloadRetry = 0;

	/* Store the process id (pid) of the clean-up thread */
	psPVRSRVData->cleanupThreadPid = OSGetCurrentProcessID();
	OSAtomicWrite(&psPVRSRVData->i32NumCleanupItems, 0);

	PVR_DPF((CLEANUP_DPFL, "CleanupThread: thread starting... "));

	/* Open an event on the clean up event object so we can listen on it,
	 * abort the clean up thread and driver if this fails.
	 */
	eRc = OSEventObjectOpen(psPVRSRVData->hCleanupEventObject, &hOSEvent);
	PVR_ASSERT(eRc == PVRSRV_OK);

	eRc = OSEventObjectOpen(psPVRSRVData->hGlobalEventObject, &hGlobalEvent);
	PVR_ASSERT(eRc == PVRSRV_OK);

	/* While the driver is in a good state and is not being unloaded
	 * try to free any deferred items when signalled
	 */
	while (psPVRSRVData->eServicesState == PVRSRV_SERVICES_STATE_OK)
	{
		IMG_HANDLE hEvent;

		if (psPVRSRVData->bUnload)
		{
			if (dllist_is_empty(&psPVRSRVData->sCleanupThreadWorkList) ||
					uiUnloadRetry > CLEANUP_THREAD_UNLOAD_RETRY)
			{
				break;
			}
			uiUnloadRetry++;
		}

		/* Wait until signalled for deferred clean up OR wait for a
		 * short period if the previous deferred clean up was not able
		 * to release all the resources before trying again.
		 * Bridge lock re-acquired on our behalf before the wait call returns.
		 */

		if (bRetryWorkList && bUseGlobalEO)
		{
			hEvent = hGlobalEvent;
		}
		else
		{
			hEvent = hOSEvent;
		}

		eRc = OSEventObjectWaitKernel(hEvent,
				bRetryWorkList ?
				CLEANUP_THREAD_WAIT_RETRY_TIMEOUT :
				CLEANUP_THREAD_WAIT_SLEEP_TIMEOUT);
		if (eRc == PVRSRV_ERROR_TIMEOUT)
		{
			PVR_DPF((CLEANUP_DPFL, "CleanupThread: wait timeout"));
		}
		else if (eRc == PVRSRV_OK)
		{
			PVR_DPF((CLEANUP_DPFL, "CleanupThread: wait OK, signal received"));
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR, "CleanupThread: wait error %d", eRc));
		}

		bRetryWorkList = _CleanupThreadProcessWorkList(psPVRSRVData, &bUseGlobalEO);
	}

	OSLockDestroy(psPVRSRVData->hCleanupThreadWorkListLock);

	eRc = OSEventObjectClose(hOSEvent);
	PVR_LOG_IF_ERROR(eRc, "OSEventObjectClose");

	eRc = OSEventObjectClose(hGlobalEvent);
	PVR_LOG_IF_ERROR(eRc, "OSEventObjectClose");

	PVR_DPF((CLEANUP_DPFL, "CleanupThread: thread ending... "));
}

static IMG_BOOL DevicesWatchdogThread_Powered_Any(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_DEV_POWER_STATE ePowerState = PVRSRV_DEV_POWER_STATE_ON;
	PVRSRV_ERROR eError;

	eError = PVRSRVPowerLock(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		if (eError == PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF)
		{
			/* Power lock cannot be acquired at this time (sys power is off) */
			return IMG_FALSE;
		}

		/* Any other error is unexpected so we assume the device is on */
		PVR_DPF((PVR_DBG_ERROR,
				 "DevicesWatchdogThread: Failed to acquire power lock for device %p (%s)",
				 psDeviceNode, PVRSRVGetErrorString(eError)));
		return IMG_TRUE;
	}

	(void) PVRSRVGetDevicePowerState(psDeviceNode, &ePowerState);

	PVRSRVPowerUnlock(psDeviceNode);

	return (ePowerState == PVRSRV_DEV_POWER_STATE_ON) ? IMG_TRUE : IMG_FALSE;
}

static void DevicesWatchdogThread_ForEachVaCb(PVRSRV_DEVICE_NODE *psDeviceNode,
											  va_list va)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice;
	PVRSRV_DEVICE_HEALTH_STATUS *pePreviousHealthStatus, eHealthStatus;
	PVRSRV_ERROR eError;

	pePreviousHealthStatus = va_arg(va, PVRSRV_DEVICE_HEALTH_STATUS *);

	if (psDeviceNode->pfnUpdateHealthStatus != NULL)
	{
		eError = psDeviceNode->pfnUpdateHealthStatus(psDeviceNode, IMG_TRUE);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_WARNING, "DevicesWatchdogThread: "
					 "Could not check for fatal error (%d)!",
					 eError));
		}
	}
	eHealthStatus = OSAtomicRead(&psDeviceNode->eHealthStatus);

	if (eHealthStatus != PVRSRV_DEVICE_HEALTH_STATUS_OK)
	{
		if (eHealthStatus != *pePreviousHealthStatus)
		{
			if (!(psDevInfo->ui32DeviceFlags &
				  RGXKM_DEVICE_STATE_DISABLE_DW_LOGGING_EN))
			{
				PVR_DPF((PVR_DBG_ERROR, "DevicesWatchdogThread: "
						 "Device status not OK!!!"));
				PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX,
								   NULL, NULL);
			}
		}
	}

	*pePreviousHealthStatus = eHealthStatus;
}

#if defined(SUPPORT_RGX)
static void HWPerfPeriodicHostEventsThread(void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = pvData;
	IMG_HANDLE hOSEvent;
	PVRSRV_ERROR eError;
	IMG_BOOL bHostStreamIsOpenForReading;
	PVRSRV_RGXDEV_INFO *psDevInfo;

	eError = OSEventObjectOpen(psPVRSRVData->hHWPerfHostPeriodicEvObj, &hOSEvent);
	PVR_LOGRN_IF_ERROR(eError, "OSEventObjectOpen");

#if defined(PVRSRV_FORCE_UNLOAD_IF_BAD_STATE)
	while ((psPVRSRVData->eServicesState == PVRSRV_SERVICES_STATE_OK) &&
			!psPVRSRVData->bUnload && !psPVRSRVData->bHWPerfHostThreadStop)
#else
	while (!psPVRSRVData->bUnload && !psPVRSRVData->bHWPerfHostThreadStop)
#endif
	{
		eError = OSEventObjectWaitKernel(hOSEvent, (IMG_UINT64)psPVRSRVData->ui32HWPerfHostThreadTimeout * 1000);
		if (eError == PVRSRV_OK && (psPVRSRVData->bUnload || psPVRSRVData->bHWPerfHostThreadStop))
		{
			PVR_DPF((PVR_DBG_MESSAGE, "HWPerfPeriodicHostEventsThread: Shutdown event received."));
			break;
		}

		psDevInfo = (PVRSRV_RGXDEV_INFO*)psPVRSRVData->psDeviceNodeList->pvDevice;

		/* Check if the HWPerf host stream is open for reading before writing a packet,
		   this covers cases where the event filter is not zeroed before a reader disconnects. */
		bHostStreamIsOpenForReading = TLStreamIsOpenForReading(psDevInfo->hHWPerfHostStream);

		if (bHostStreamIsOpenForReading)
		{
#if defined(SUPPORT_RGX)
			RGXSRV_HWPERF_HOST_INFO(psPVRSRVData->psDeviceNodeList->pvDevice, RGX_HWPERF_INFO_EV_MEM_USAGE);
#endif
		}
		else
		{
#if defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP)
			psPVRSRVData->ui32HWPerfHostThreadTimeout = INFINITE_SLEEP_TIMEOUT;
#else
			/* This 'long' timeout is temporary until functionality is added to services to put a thread to sleep indefinitely. */
			psPVRSRVData->ui32HWPerfHostThreadTimeout = 60 * 60 * 8 * 1000; // 8 hours
#endif
		}
	}

	eError = OSEventObjectClose(hOSEvent);
	PVR_LOG_IF_ERROR(eError, "OSEventObjectClose");
}
#endif

#if defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP)

typedef enum
{
	DWT_ST_INIT,
	DWT_ST_SLEEP_POWERON,
	DWT_ST_SLEEP_POWEROFF,
	DWT_ST_SLEEP_DEFERRED,
	DWT_ST_FINAL
} DWT_STATE;

typedef enum
{
	DWT_SIG_POWERON,
	DWT_SIG_POWEROFF,
	DWT_SIG_TIMEOUT,
	DWT_SIG_UNLOAD,
	DWT_SIG_ERROR
} DWT_SIGNAL;

static inline IMG_BOOL _DwtIsPowerOn(PVRSRV_DATA *psPVRSRVData)
{
	return List_PVRSRV_DEVICE_NODE_IMG_BOOL_Any(psPVRSRVData->psDeviceNodeList,
	                                         DevicesWatchdogThread_Powered_Any);
}

static inline void _DwtCheckHealthStatus(PVRSRV_DATA *psPVRSRVData,
                                         PVRSRV_DEVICE_HEALTH_STATUS *peStatus)
{
	List_PVRSRV_DEVICE_NODE_ForEach_va(psPVRSRVData->psDeviceNodeList,
	                                   DevicesWatchdogThread_ForEachVaCb,
	                                   peStatus);

#if defined(SUPPORT_GPUVIRT_VALIDATION) && defined(EMULATOR)
	SysPrintAndResetFaultStatusRegister();
#endif
}

static DWT_SIGNAL _DwtWait(PVRSRV_DATA *psPVRSRVData, IMG_HANDLE hOSEvent,
                           IMG_UINT32 ui32Timeout)
{
	PVRSRV_ERROR eError;

	eError = OSEventObjectWaitKernel(hOSEvent, (IMG_UINT64) ui32Timeout * 1000);

#ifdef PVR_TESTING_UTILS
	psPVRSRVData->ui32DevicesWdWakeupCounter++;
#endif

	if (eError == PVRSRV_OK)
	{
		if (psPVRSRVData->bUnload)
		{
			PVR_DPF((PVR_DBG_MESSAGE, "DevicesWatchdogThread: Shutdown event"
			        " received."));
			return DWT_SIG_UNLOAD;
		}
		else
		{
			PVR_DPF((PVR_DBG_MESSAGE, "DevicesWatchdogThread: Power state "
			        "change event received."));

			if (_DwtIsPowerOn(psPVRSRVData))
			{
				return DWT_SIG_POWERON;
			}
			else
			{
				return DWT_SIG_POWEROFF;
			}
		}
	}
	else if (eError == PVRSRV_ERROR_TIMEOUT)
	{
		return DWT_SIG_TIMEOUT;
	}

	PVR_DPF((PVR_DBG_ERROR, "DevicesWatchdogThread: Error (%d) when"
	        " waiting for event!", eError));
	return DWT_SIG_ERROR;
}

#endif /* defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP) */

static void DevicesWatchdogThread(void *pvData)
{
	PVRSRV_DATA *psPVRSRVData = pvData;
	PVRSRV_DEVICE_HEALTH_STATUS ePreviousHealthStatus = PVRSRV_DEVICE_HEALTH_STATUS_OK;
	IMG_HANDLE hOSEvent;
	PVRSRV_ERROR eError;
#if defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP)
	DWT_STATE eState = DWT_ST_INIT;
	const IMG_UINT32 ui32OnTimeout = DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT;
	const IMG_UINT32 ui32OffTimeout = INFINITE_SLEEP_TIMEOUT;
#else
	IMG_UINT32 ui32Timeout = DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT;
	/* Flag used to defer the sleep timeout change by 1 loop iteration.
	 * This helps to ensure at least two health checks are performed before a long sleep.
	 */
	IMG_BOOL bDoDeferredTimeoutChange = IMG_FALSE;
#endif

	PVR_DPF((PVR_DBG_MESSAGE, "DevicesWatchdogThread: Power off sleep time: %d.",
			DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT));

	/* Open an event on the devices watchdog event object so we can listen on it
	   and abort the devices watchdog thread. */
	eError = OSEventObjectOpen(psPVRSRVData->hDevicesWatchdogEvObj, &hOSEvent);
	PVR_LOGRN_IF_ERROR(eError, "OSEventObjectOpen");

	/* Loop continuously checking the device status every few seconds. */
#if defined(PVRSRV_FORCE_UNLOAD_IF_BAD_STATE)
	while ((psPVRSRVData->eServicesState == PVRSRV_SERVICES_STATE_OK) &&
			!psPVRSRVData->bUnload)
#else
	while (!psPVRSRVData->bUnload)
#endif
	{
#if defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP)

		switch (eState)
		{
			case DWT_ST_INIT:
			{
				if (_DwtIsPowerOn(psPVRSRVData))
				{
					eState = DWT_ST_SLEEP_POWERON;
				}
				else
				{
					eState = DWT_ST_SLEEP_POWEROFF;
				}

				break;
			}
			case DWT_ST_SLEEP_POWERON:
			{
				DWT_SIGNAL eSignal = _DwtWait(psPVRSRVData, hOSEvent,
				                                    ui32OnTimeout);

				switch (eSignal) {
					case DWT_SIG_POWERON:
						/* self-transition, nothing to do */
						break;
					case DWT_SIG_POWEROFF:
						eState = DWT_ST_SLEEP_DEFERRED;
						break;
					case DWT_SIG_TIMEOUT:
						_DwtCheckHealthStatus(psPVRSRVData,
						                      &ePreviousHealthStatus);
						/* self-transition */
						break;
					case DWT_SIG_UNLOAD:
						eState = DWT_ST_FINAL;
						break;
					case DWT_SIG_ERROR:
						/* deliberately ignored */
						break;
				}

				break;
			}
			case DWT_ST_SLEEP_POWEROFF:
			{
				DWT_SIGNAL eSignal = _DwtWait(psPVRSRVData, hOSEvent,
				                                    ui32OffTimeout);

				switch (eSignal) {
					case DWT_SIG_POWERON:
						eState = DWT_ST_SLEEP_POWERON;
						_DwtCheckHealthStatus(psPVRSRVData,
						                      &ePreviousHealthStatus);
						break;
					case DWT_SIG_POWEROFF:
						/* self-transition, nothing to do */
						break;
					case DWT_SIG_TIMEOUT:
						/* self-transition */
						_DwtCheckHealthStatus(psPVRSRVData,
						                      &ePreviousHealthStatus);
						break;
					case DWT_SIG_UNLOAD:
						eState = DWT_ST_FINAL;
						break;
					case DWT_SIG_ERROR:
						/* deliberately ignored */
						break;
				}

				break;
			}
			case DWT_ST_SLEEP_DEFERRED:
			{
				DWT_SIGNAL eSignal =_DwtWait(psPVRSRVData, hOSEvent,
				                                   ui32OnTimeout);

				switch (eSignal) {
					case DWT_SIG_POWERON:
						eState = DWT_ST_SLEEP_POWERON;
						_DwtCheckHealthStatus(psPVRSRVData,
						                      &ePreviousHealthStatus);
						break;
					case DWT_SIG_POWEROFF:
						/* self-transition, nothing to do */
						break;
					case DWT_SIG_TIMEOUT:
						eState = DWT_ST_SLEEP_POWEROFF;
						_DwtCheckHealthStatus(psPVRSRVData,
						                      &ePreviousHealthStatus);
						break;
					case DWT_SIG_UNLOAD:
						eState = DWT_ST_FINAL;
						break;
					case DWT_SIG_ERROR:
						/* deliberately ignored */
						break;
				}

				break;
			}
			case DWT_ST_FINAL:
				/* the loop should terminate on next spin if this state is
				 * reached so nothing to do here. */
				break;
		}

#else /* defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP) */
		IMG_BOOL bPwrIsOn = IMG_FALSE;

		/* Wait time between polls (done at the start of the loop to allow devices
		   to initialise) or for the event signal (shutdown or power on). */
		eError = OSEventObjectWaitKernel(hOSEvent, (IMG_UINT64)ui32Timeout * 1000);

#ifdef PVR_TESTING_UTILS
		psPVRSRVData->ui32DevicesWdWakeupCounter++;
#endif
		if (eError == PVRSRV_OK)
		{
			if (psPVRSRVData->bUnload)
			{
				PVR_DPF((PVR_DBG_MESSAGE, "DevicesWatchdogThread: Shutdown event received."));
				break;
			}
			else
			{
				PVR_DPF((PVR_DBG_MESSAGE, "DevicesWatchdogThread: Power state change event received."));
			}
		}
		else if (eError != PVRSRV_ERROR_TIMEOUT)
		{
			/* If timeout do nothing otherwise print warning message. */
			PVR_DPF((PVR_DBG_ERROR, "DevicesWatchdogThread: "
					"Error (%d) when waiting for event!", eError));
		}

		bPwrIsOn = List_PVRSRV_DEVICE_NODE_IMG_BOOL_Any(psPVRSRVData->psDeviceNodeList,
														DevicesWatchdogThread_Powered_Any);

		if (bPwrIsOn || psPVRSRVData->ui32DevicesWatchdogPwrTrans)
		{
			psPVRSRVData->ui32DevicesWatchdogPwrTrans = 0;
			ui32Timeout = psPVRSRVData->ui32DevicesWatchdogTimeout = DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT;
			bDoDeferredTimeoutChange = IMG_FALSE;
		}
		else
		{
			/* First, check if the previous loop iteration signalled a need to change the timeout period */
			if (bDoDeferredTimeoutChange)
			{
				ui32Timeout = psPVRSRVData->ui32DevicesWatchdogTimeout = DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT;
				bDoDeferredTimeoutChange = IMG_FALSE;
			}
			else
			{
				/* Signal that we need to change the sleep timeout in the next loop iteration
				 * to allow the device health check code a further iteration at the current
				 * sleep timeout in order to determine bad health (e.g. stalled cCCB) by
				 * comparing past and current state snapshots */
				bDoDeferredTimeoutChange = IMG_TRUE;
			}
		}

		List_PVRSRV_DEVICE_NODE_ForEach_va(psPVRSRVData->psDeviceNodeList,
										   DevicesWatchdogThread_ForEachVaCb,
										   &ePreviousHealthStatus);


#if defined(SUPPORT_GPUVIRT_VALIDATION) && defined(EMULATOR)
		SysPrintAndResetFaultStatusRegister();
#endif

#endif /* defined(PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP) */
	}

	eError = OSEventObjectClose(hOSEvent);
	PVR_LOG_IF_ERROR(eError, "OSEventObjectClose");
}

PVRSRV_DATA *PVRSRVGetPVRSRVData(void)
{
	return gpsPVRSRVData;
}

static PVRSRV_ERROR _HostMemDeviceCreate(void)
{
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_DEVICE_CONFIG *psDevConfig = HostMemGetDeviceConfig();
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	/* Assert ensures HostMemory device isn't already created and
	 * that data is initialized */
	PVR_ASSERT(psPVRSRVData->psHostMemDeviceNode == NULL);

	/* for now, we only know a single heap (UMA) config for host device */
	PVR_ASSERT(psDevConfig->ui32PhysHeapCount == 1 &&
				psDevConfig->pasPhysHeaps[0].eType == PHYS_HEAP_TYPE_UMA);

	/* N.B.- In case of any failures in this function, we just return error to
	   the caller, as clean-up is taken care by _HostMemDeviceDestroy function */

	psDeviceNode = OSAllocZMem(sizeof(*psDeviceNode));
	PVR_LOGR_IF_NOMEM(psDeviceNode, "OSAllocZMem");

	/* early save return pointer to aid clean-up */
	psPVRSRVData->psHostMemDeviceNode = psDeviceNode;

	psDeviceNode->psDevConfig = psDevConfig;
	psDeviceNode->papsRegisteredPhysHeaps =
		OSAllocZMem(sizeof(*psDeviceNode->papsRegisteredPhysHeaps) *
					psDevConfig->ui32PhysHeapCount);
	PVR_LOGR_IF_NOMEM(psDeviceNode->papsRegisteredPhysHeaps, "OSAllocZMem");

	eError = PhysHeapRegister(&psDevConfig->pasPhysHeaps[0],
								  &psDeviceNode->papsRegisteredPhysHeaps[0]);
	PVR_LOGR_IF_ERROR(eError, "PhysHeapRegister");
	psDeviceNode->ui32RegisteredPhysHeaps = 1;

	/* Only CPU local heap is valid on host-mem DevNode, so enable minimal callbacks */
	eError = PhysHeapAcquire(psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL],
							 &psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL]);
	PVR_LOGR_IF_ERROR(eError, "PhysHeapAcquire");

	psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL] = PhysmemNewOSRamBackedPMR;

	return PVRSRV_OK;
}

static void _HostMemDeviceDestroy(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE *psDeviceNode = psPVRSRVData->psHostMemDeviceNode;

	if (!psDeviceNode)
	{
		return;
	}

	psPVRSRVData->psHostMemDeviceNode = NULL;
	if (psDeviceNode->papsRegisteredPhysHeaps)
	{
		if (psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL])
		{
			PhysHeapRelease(psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL]);
		}

		if (psDeviceNode->papsRegisteredPhysHeaps[0])
		{
			/* clean-up function as well is aware of only one heap */
			PVR_ASSERT(psDeviceNode->ui32RegisteredPhysHeaps == 1);
			PhysHeapUnregister(psDeviceNode->papsRegisteredPhysHeaps[0]);
		}

		OSFreeMem(psDeviceNode->papsRegisteredPhysHeaps);
	}
	OSFreeMem(psDeviceNode);
}

static PVRSRV_ERROR InitialiseInfoPageTimeouts(PVRSRV_DATA *psPVRSRVData)
{
	if (NULL == psPVRSRVData)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psPVRSRVData->pui32InfoPage[TIMEOUT_INFO_VALUE_RETRIES] = WAIT_TRY_COUNT;
	psPVRSRVData->pui32InfoPage[TIMEOUT_INFO_VALUE_TIMEOUT_MS] =
		((MAX_HW_TIME_US / 10000) + 1000);
		/* TIMEOUT_INFO_VALUE_TIMEOUT_MS resolves to...
			vp       : 2000  + 1000
			emu      : 2000  + 1000
			rgx_nohw : 50    + 1000
			plato    : 30000 + 1000 (VIRTUAL_PLATFORM or EMULATOR)
			           50    + 1000 (otherwise)
		*/

	psPVRSRVData->pui32InfoPage[TIMEOUT_INFO_CONDITION_RETRIES] = 5;
	psPVRSRVData->pui32InfoPage[TIMEOUT_INFO_CONDITION_TIMEOUT_MS] =
		((MAX_HW_TIME_US / 10000) + 100);
		/* TIMEOUT_INFO_CONDITION_TIMEOUT_MS resolves to...
			vp       : 2000  + 100
			emu      : 2000  + 100
			rgx_nohw : 50    + 100
			plato    : 30000 + 100 (VIRTUAL_PLATFORM or EMULATOR)
			           50    + 100 (otherwise)
		*/

	psPVRSRVData->pui32InfoPage[TIMEOUT_INFO_EVENT_OBJECT_RETRIES] = 5;
	psPVRSRVData->pui32InfoPage[TIMEOUT_INFO_EVENT_OBJECT_TIMEOUT_MS] =
		((MAX_HW_TIME_US / 10000) + 100);
		/* TIMEOUT_INFO_EVENT_OBJECT_TIMEOUT_MS resolves to...
			vp       : 2000  + 100
			emu      : 2000  + 100
			rgx_nohw : 50    + 100
			plato    : 30000 + 100 (VIRTUAL_PLATFORM or EMULATOR)
			           50    + 100 (otherwise)
		*/

	return PVRSRV_OK;
}

PVRSRV_ERROR IMG_CALLCONV
PVRSRVDriverInit(void)
{
	PVRSRV_ERROR eError;

	PVRSRV_DATA	*psPVRSRVData = NULL;

	IMG_UINT32 ui32AppHintCleanupThreadPriority;
	IMG_UINT32 ui32AppHintWatchdogThreadPriority;
	IMG_BOOL bEnablePageFaultDebug;
	IMG_BOOL bEnableFullSyncTracking;

	void *pvAppHintState = NULL;
	IMG_UINT32 ui32AppHintDefault;

	/*
	 * As this function performs one time driver initialisation, use the
	 * Services global device-independent data to determine whether or not
	 * this function has already been called.
	 */
	if (gpsPVRSRVData)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Driver already initialised", __func__));
		return PVRSRV_ERROR_ALREADY_EXISTS;
	}

	/*
	 * Initialise the server bridges
	 */
	eError = ServerBridgeInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	eError = PhysHeapInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	eError = DevmemIntInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	/*
	 * Allocate the device-independent data
	 */
	psPVRSRVData = OSAllocZMem(sizeof(*gpsPVRSRVData));
	if (psPVRSRVData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto Error;
	}

	/* Now it is set up, point gpsPVRSRVData to the actual data */
	gpsPVRSRVData = psPVRSRVData;

	eError = BridgeDispatcherInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	/* Init any OS specific's */
	eError = OSInitEnvData();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	/* Early init. server cache maintenance */
	eError = CacheOpInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
	RIInitKM();
#endif

	ui32AppHintDefault = PVRSRV_APPHINT_ENABLEPAGEFAULTDEBUG;

	OSCreateKMAppHintState(&pvAppHintState);
	OSGetKMAppHintBOOL(pvAppHintState, EnablePageFaultDebug,
			&ui32AppHintDefault, &bEnablePageFaultDebug);
	OSFreeKMAppHintState(pvAppHintState);

	if (bEnablePageFaultDebug)
	{
		eError = DevicememHistoryInitKM();

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
						"%s: Failed to initialise DevicememHistoryInitKM", __func__));
			goto Error;
		}
	}

	eError = PMRInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

#if defined(SUPPORT_DISPLAY_CLASS)
	eError = DCInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}
#endif

	/* Initialise overall system state */
	gpsPVRSRVData->eServicesState = PVRSRV_SERVICES_STATE_OK;

	/* Create an event object */
	eError = OSEventObjectCreate("PVRSRV_GLOBAL_EVENTOBJECT", &gpsPVRSRVData->hGlobalEventObject);
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}
	gpsPVRSRVData->ui32GEOConsecutiveTimeouts = 0;

	eError = PVRSRVCmdCompleteInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	eError = PVRSRVHandleInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	OSCreateKMAppHintState(&pvAppHintState);
	ui32AppHintDefault = PVRSRV_APPHINT_CLEANUPTHREADPRIORITY;
	OSGetKMAppHintUINT32(pvAppHintState, CleanupThreadPriority,
	                     &ui32AppHintDefault, &ui32AppHintCleanupThreadPriority);

	ui32AppHintDefault = PVRSRV_APPHINT_WATCHDOGTHREADPRIORITY;
	OSGetKMAppHintUINT32(pvAppHintState, WatchdogThreadPriority,
	                     &ui32AppHintDefault, &ui32AppHintWatchdogThreadPriority);

	ui32AppHintDefault = PVRSRV_APPHINT_ENABLEFULLSYNCTRACKING;
	OSGetKMAppHintBOOL(pvAppHintState, EnableFullSyncTracking,
			&ui32AppHintDefault, &bEnableFullSyncTracking);
	OSFreeKMAppHintState(pvAppHintState);
	pvAppHintState = NULL;

	eError = _CleanupThreadPrepare(gpsPVRSRVData);
	PVR_LOGG_IF_ERROR(eError, "_CleanupThreadPrepare", Error);

	/* Create a thread which is used to do the deferred cleanup */
	eError = OSThreadCreatePriority(&gpsPVRSRVData->hCleanupThread,
	                                "pvr_defer_free",
	                                CleanupThread,
	                                CleanupThreadDumpInfo,
	                                IMG_TRUE,
	                                gpsPVRSRVData,
	                                ui32AppHintCleanupThreadPriority);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create deferred cleanup thread",
				 __func__));
		goto Error;
	}

	/* Create the devices watchdog event object */
	eError = OSEventObjectCreate("PVRSRV_DEVICESWATCHDOG_EVENTOBJECT", &gpsPVRSRVData->hDevicesWatchdogEvObj);
	PVR_LOGG_IF_ERROR(eError, "OSEventObjectCreate", Error);

	/* Create a thread which is used to detect fatal errors */
	eError = OSThreadCreatePriority(&gpsPVRSRVData->hDevicesWatchdogThread,
	                                "pvr_device_wdg",
	                                DevicesWatchdogThread,
	                                NULL,
	                                IMG_TRUE,
	                                gpsPVRSRVData,
	                                ui32AppHintWatchdogThreadPriority);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create devices watchdog thread",
				 __func__));
		goto Error;
	}

	gpsPVRSRVData->psProcessHandleBase_Table = HASH_Create(PVRSRV_PROC_HANDLE_BASE_INIT);

	if (gpsPVRSRVData->psProcessHandleBase_Table == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Failed to create hash table for process handle base.",
				__func__));
		eError = PVRSRV_ERROR_UNABLE_TO_CREATE_HASH_TABLE;
		goto Error;
	}

	eError = OSLockCreate(&gpsPVRSRVData->hProcessHandleBase_Lock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Failed to create lock for process handle base.",
				__func__));
		goto Error;
	}

#if defined(SUPPORT_RGX)
	eError = OSLockCreate(&gpsPVRSRVData->hHWPerfHostPeriodicThread_Lock);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Failed to create lock for HWPerf periodic thread.",
				__func__));
		goto Error;
	}
#endif

	eError = _HostMemDeviceCreate();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	/* Initialise the Transport Layer */
	eError = TLInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	/* Initialise pdump */
	eError = PDUMPINIT();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	g_ui32InitFlags |= INIT_DATA_ENABLE_PDUMPINIT;

	/* Initialise TL control stream */
	eError = TLStreamCreate(&psPVRSRVData->hTLCtrlStream,
	                        psPVRSRVData->psHostMemDeviceNode,
	                        PVRSRV_TL_CTLR_STREAM, PVRSRV_TL_CTLR_STREAM_SIZE,
	                        TL_OPMODE_DROP_OLDEST, NULL, NULL, NULL,
                            NULL);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to create TL control plane stream"
		        " (%d).", eError));
		psPVRSRVData->hTLCtrlStream = NULL;
	}

	eError = InfoPageCreate(psPVRSRVData);
	PVR_LOGG_IF_ERROR(eError, "InfoPageCreate", Error);


	/* Initialise the Timeout Info */
	eError = InitialiseInfoPageTimeouts(psPVRSRVData);
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

	if (bEnableFullSyncTracking)
	{
		psPVRSRVData->pui32InfoPage[DEBUG_FEATURE_FLAGS] |= DEBUG_FEATURE_FULL_SYNC_TRACKING_ENABLED;
	}
	if (bEnablePageFaultDebug)
	{
		psPVRSRVData->pui32InfoPage[DEBUG_FEATURE_FLAGS] |= DEBUG_FEATURE_PAGE_FAULT_DEBUG_ENABLED;
	}

	/* Initialise the Host Trace Buffer */
	eError = HTBInit();
	if (eError != PVRSRV_OK)
	{
		goto Error;
	}

#if defined(SUPPORT_RGX)
	RGXHWPerfClientInitAppHintCallbacks();
#endif

	/* Late init. client cache maintenance via info. page */
	eError = CacheOpInit2();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: failed to initialise the CacheOp framework (%d)",
				__func__, eError));
		goto Error;
	}
#if defined(SUPPORT_SERVER_SYNC_IMPL)
	eError = ServerSyncInitOnce(psPVRSRVData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Failed to initialise sync server",
				__func__));
		goto Error;
	}
#endif

	dllist_init(&psPVRSRVData->sConnections);
	eError = OSLockCreate(&psPVRSRVData->hConnectionsLock);
	PVR_LOGG_IF_ERROR(eError, "OSLockCreate", Error);

	return 0;

Error:
	PVRSRVDriverDeInit();
	return eError;
}

void IMG_CALLCONV
PVRSRVDriverDeInit(void)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_BOOL bEnablePageFaultDebug;

	if (gpsPVRSRVData == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: missing device-independent data",
				 __func__));
		return;
	}

	bEnablePageFaultDebug = GetInfoPageDebugFlagsKM() & DEBUG_FEATURE_PAGE_FAULT_DEBUG_ENABLED;

	gpsPVRSRVData->bUnload = IMG_TRUE;

	if (gpsPVRSRVData->hProcessHandleBase_Lock)
	{
		OSLockDestroy(gpsPVRSRVData->hProcessHandleBase_Lock);
		gpsPVRSRVData->hProcessHandleBase_Lock = NULL;
	}

#if defined(SUPPORT_RGX)
	PVRSRVDestroyHWPerfHostThread();
	if (gpsPVRSRVData->hHWPerfHostPeriodicThread_Lock)
	{
		OSLockDestroy(gpsPVRSRVData->hHWPerfHostPeriodicThread_Lock);
		gpsPVRSRVData->hHWPerfHostPeriodicThread_Lock = NULL;
	}
#endif

	if (gpsPVRSRVData->psProcessHandleBase_Table)
	{
		HASH_Delete(gpsPVRSRVData->psProcessHandleBase_Table);
		gpsPVRSRVData->psProcessHandleBase_Table = NULL;
	}

	if (gpsPVRSRVData->hGlobalEventObject)
	{
		OSEventObjectSignal(gpsPVRSRVData->hGlobalEventObject);
	}

	/* Stop and cleanup the devices watchdog thread */
	if (gpsPVRSRVData->hDevicesWatchdogThread)
	{
		if (gpsPVRSRVData->hDevicesWatchdogEvObj)
		{
			eError = OSEventObjectSignal(gpsPVRSRVData->hDevicesWatchdogEvObj);
			PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");
		}
		LOOP_UNTIL_TIMEOUT(OS_THREAD_DESTROY_TIMEOUT_US)
		{
			eError = OSThreadDestroy(gpsPVRSRVData->hDevicesWatchdogThread);
			if (PVRSRV_OK == eError)
			{
				gpsPVRSRVData->hDevicesWatchdogThread = NULL;
				break;
			}
			OSWaitus(OS_THREAD_DESTROY_TIMEOUT_US/OS_THREAD_DESTROY_RETRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();
		PVR_LOG_IF_ERROR(eError, "OSThreadDestroy");
	}

	if (gpsPVRSRVData->hDevicesWatchdogEvObj)
	{
		eError = OSEventObjectDestroy(gpsPVRSRVData->hDevicesWatchdogEvObj);
		gpsPVRSRVData->hDevicesWatchdogEvObj = NULL;
		PVR_LOG_IF_ERROR(eError, "OSEventObjectDestroy");
	}

	/* Stop and cleanup the deferred clean up thread, event object and
	 * deferred context list.
	 */
	if (gpsPVRSRVData->hCleanupThread)
	{
		if (gpsPVRSRVData->hCleanupEventObject)
		{
			eError = OSEventObjectSignal(gpsPVRSRVData->hCleanupEventObject);
			PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");
		}
		LOOP_UNTIL_TIMEOUT(OS_THREAD_DESTROY_TIMEOUT_US)
		{
			eError = OSThreadDestroy(gpsPVRSRVData->hCleanupThread);
			if (PVRSRV_OK == eError)
			{
				gpsPVRSRVData->hCleanupThread = NULL;
				break;
			}
			OSWaitus(OS_THREAD_DESTROY_TIMEOUT_US/OS_THREAD_DESTROY_RETRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();
		PVR_LOG_IF_ERROR(eError, "OSThreadDestroy");
	}

	if (gpsPVRSRVData->hCleanupEventObject)
	{
		eError = OSEventObjectDestroy(gpsPVRSRVData->hCleanupEventObject);
		gpsPVRSRVData->hCleanupEventObject = NULL;
		PVR_LOG_IF_ERROR(eError, "OSEventObjectDestroy");
	}

	/* Tear down the HTB before PVRSRVHandleDeInit() removes its TL handle */
	/* HTB De-init happens in device de-registration currently */
	eError = HTBDeInit();
	PVR_LOG_IF_ERROR(eError, "HTBDeInit");

	/* Tear down CacheOp framework information page first */
	CacheOpDeInit2();

#if defined(SUPPORT_SERVER_SYNC_IMPL)
	ServerSyncDeinitOnce(gpsPVRSRVData);
#endif
	/* Clean up information page */
	InfoPageDestroy(gpsPVRSRVData);

	/* Close the TL control plane stream. */
	TLStreamClose(gpsPVRSRVData->hTLCtrlStream);

	/* deinitialise pdump */
	if ((g_ui32InitFlags & INIT_DATA_ENABLE_PDUMPINIT) > 0)
	{
		PDUMPDEINIT();
	}

	/* Clean up Transport Layer resources that remain */
	TLDeInit();

	_HostMemDeviceDestroy();

	eError = PVRSRVHandleDeInit();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: PVRSRVHandleDeInit failed", __func__));
	}

	/* destroy event object */
	if (gpsPVRSRVData->hGlobalEventObject)
	{
		OSEventObjectDestroy(gpsPVRSRVData->hGlobalEventObject);
		gpsPVRSRVData->hGlobalEventObject = NULL;
	}

	PVRSRVCmdCompleteDeinit();

#if defined(SUPPORT_DISPLAY_CLASS)
	eError = DCDeInit();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: DCDeInit failed", __func__));
	}
#endif

	eError = PMRDeInit();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: PMRDeInit failed", __func__));
	}

	BridgeDispatcherDeinit();

#if defined(PVRSRV_ENABLE_GPU_MEMORY_INFO)
	RIDeInitKM();
#endif

	if (bEnablePageFaultDebug)
	{
		DevicememHistoryDeInitKM();
	}

	CacheOpDeInit();

	OSDeInitEnvData();

	(void) DevmemIntDeInit();

	eError = ServerBridgeDeInit();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: ServerBridgeDeinit failed", __func__));
	}

	eError = PhysHeapDeinit();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: PhysHeapDeinit failed", __func__));
	}

	if (OSLockDestroy(gpsPVRSRVData->hConnectionsLock) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: ConnectionLock destruction failed", __func__));
	}

	OSFreeMem(gpsPVRSRVData);
	gpsPVRSRVData = NULL;
}

#if defined(SUPPORT_GPUVIRT_VALIDATION)
static PVRSRV_ERROR CreateLMASubArenas(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	IMG_UINT	uiCounter=0;

	for (uiCounter = 0; uiCounter < GPUVIRT_VALIDATION_NUM_OS; uiCounter++)
	{
		psDeviceNode->psOSidSubArena[uiCounter] =
			RA_Create(psDeviceNode->apszRANames[0],
					  OSGetPageShift(),			/* Use host page size, keeps things simple */
					  RA_LOCKCLASS_0,			/* This arena doesn't use any other arenas. */
					  NULL,					/* No Import */
					  NULL,					/* No free import */
					  NULL,					/* No import handle */
					  IMG_FALSE);

		if (psDeviceNode->psOSidSubArena[uiCounter] == NULL)
		{
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
	}

	PVR_DPF((PVR_DBG_MESSAGE,"(GPU Virtualization Validation): Calling RA_Add with base %u and size %u",0, GPUVIRT_SIZEOF_ARENA0));

	/* Arena creation takes place earlier than when the client side reads the apphints and transfers them over the bridge. Since we don't
	 * know how the memory is going to be partitioned and since we already need some memory for all the initial allocations that take place,
	 * we populate the first sub-arena (0) with a span of 64 megabytes. This has been shown to be enough even for cases where EWS is allocated
	 * memory in this sub arena and then a multi app example is executed. This pre-allocation also means that consistency must be maintained
	 * between apphints and reality. That's why in the Apphints, the OSid0 region must start from 0 and end at 3FFFFFF. */

	if (!RA_Add(psDeviceNode->psOSidSubArena[0], 0, GPUVIRT_SIZEOF_ARENA0, 0 , NULL))
	{
		RA_Delete(psDeviceNode->psOSidSubArena[0]);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psDeviceNode->apsLocalDevMemArenas[0] = psDeviceNode->psOSidSubArena[0];

	return PVRSRV_OK;
}

void PopulateLMASubArenas(PVRSRV_DEVICE_NODE *psDeviceNode,
						  IMG_UINT32 aui32OSidMin[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS],
						  IMG_UINT32 aui32OSidMax[GPUVIRT_VALIDATION_NUM_REGIONS][GPUVIRT_VALIDATION_NUM_OS])
{
	IMG_UINT	uiCounter;

	/* Since Sub Arena[0] has been populated already, now we populate the rest starting from 1*/

	for (uiCounter = 1; uiCounter < GPUVIRT_VALIDATION_NUM_OS; uiCounter++)
	{
		PVR_DPF((PVR_DBG_MESSAGE,"(GPU Virtualization Validation): Calling RA_Add with base %u and size %u",aui32OSidMin[0][uiCounter], aui32OSidMax[0][uiCounter]-aui32OSidMin[0][uiCounter]+1));

		if (!RA_Add(psDeviceNode->psOSidSubArena[uiCounter], aui32OSidMin[0][uiCounter], aui32OSidMax[0][uiCounter]-aui32OSidMin[0][uiCounter]+1, 0, NULL))
		{
			goto error;
		}
	}

	#if defined(EMULATOR)
	{
		SysSetOSidRegisters(aui32OSidMin, aui32OSidMax);
	}
	#endif

	return;

error:
	for (uiCounter = 0; uiCounter < GPUVIRT_VALIDATION_NUM_OS; uiCounter++)
	{
		RA_Delete(psDeviceNode->psOSidSubArena[uiCounter]);
	}

	return;
}

#endif

static void _SysDebugRequestNotify(PVRSRV_DBGREQ_HANDLE hDebugRequestHandle,
					IMG_UINT32 ui32VerbLevel,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	/* Only dump info once */
	PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE*) hDebugRequestHandle;

	switch (psDeviceNode->eCurrentSysPowerState)
	{
		case PVRSRV_SYS_POWER_STATE_OFF:
			PVR_DUMPDEBUG_LOG("Device System Power State: OFF");
			break;
		case PVRSRV_SYS_POWER_STATE_ON:
			PVR_DUMPDEBUG_LOG("Device System Power State: ON");
			break;
		default:
			PVR_DUMPDEBUG_LOG("Device System Power State: UNKNOWN (%d)",
							   psDeviceNode->eCurrentSysPowerState);
			break;
	}

	SysDebugInfo(psDeviceNode->psDevConfig, pfnDumpDebugPrintf, pvDumpDebugFile);
}

static void _ThreadsDebugRequestNotify(PVRSRV_DBGREQ_HANDLE hDbgReqestHandle,
                                       IMG_UINT32 ui32VerbLevel,
                                       DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                       void *pvDumpDebugFile)
{
	PVR_UNREFERENCED_PARAMETER(hDbgReqestHandle);

	if (DD_VERB_LVL_ENABLED(ui32VerbLevel, DEBUG_REQUEST_VERBOSITY_HIGH))
	{
		PVR_DUMPDEBUG_LOG("------[ Server Thread Summary ]------");
		OSThreadDumpInfo(pfnDumpDebugPrintf, pvDumpDebugFile);
	}
}

PVRSRV_ERROR PVRSRVPhysMemHeapsInit(PVRSRV_DEVICE_NODE *psDeviceNode, PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_DEVICE_PHYS_HEAP physHeapIndex;
	IMG_UINT32 ui32RegionId = 0;
	PVRSRV_ERROR eError;
	IMG_UINT32 i;

	/* Register the physical memory heaps */
	psDeviceNode->papsRegisteredPhysHeaps =
		OSAllocZMem(sizeof(*psDeviceNode->papsRegisteredPhysHeaps) *
					psDevConfig->ui32PhysHeapCount);
	if (!psDeviceNode->papsRegisteredPhysHeaps)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	for (i = 0; i < psDevConfig->ui32PhysHeapCount; i++)
	{
		/* No real device should register a heap with ID same as host device's heap ID */
		PVR_ASSERT(psDevConfig->pasPhysHeaps[i].ui32PhysHeapID != PHYS_HEAP_ID_HOSTMEM);

		eError = PhysHeapRegister(&psDevConfig->pasPhysHeaps[i],
								  &psDeviceNode->papsRegisteredPhysHeaps[i]);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to register physical heap %d (%s)",
					 __func__, psDevConfig->pasPhysHeaps[i].ui32PhysHeapID,
					 PVRSRVGetErrorString(eError)));
			goto ErrorPhysHeapsUnregister;
		}

		psDeviceNode->ui32RegisteredPhysHeaps++;
	}

	/*
	 * The physical backing storage for the following physical heaps
	 * [CPU,GPU,FW] may or may not come from the same underlying source
	 */
	eError = PhysHeapAcquire(psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL],
							 &psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL]);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to acquire PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL physical memory heap",
				 __func__));
		goto ErrorPhysHeapsUnregister;
	}

	eError = PhysHeapAcquire(psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL],
							 &psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL]);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to acquire PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL physical memory heap",
				 __func__));
		goto ErrorPhysHeapsRelease;
	}

	eError = PhysHeapAcquire(psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL],
							 &psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL]);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to acquire PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL physical memory heap",
				 __func__));
		goto ErrorPhysHeapsRelease;
	}

	eError = PhysHeapAcquire(psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_EXTERNAL],
							 &psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_EXTERNAL]);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to acquire PVRSRV_DEVICE_PHYS_HEAP_EXTERNAL physical memory heap",
				 __func__));
		goto ErrorPhysHeapsRelease;
	}

	/* Do we have card memory? If so create RAs to manage it */
	if (PhysHeapGetType(psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL]) == PHYS_HEAP_TYPE_LMA)
	{
		RA_BASE_T uBase;
		RA_LENGTH_T uSize;
		IMG_UINT64 ui64Size;
		IMG_CPU_PHYADDR sCpuPAddr;
		IMG_DEV_PHYADDR sDevPAddr;

		IMG_UINT32 ui32NumOfLMARegions;
		PHYS_HEAP* psLMAHeap;

		psLMAHeap = psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL];
		ui32NumOfLMARegions = PhysHeapNumberOfRegions(psLMAHeap);

		if (ui32NumOfLMARegions == 0)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: LMA heap has no memory regions defined.", __func__));
			eError = PVRSRV_ERROR_DEVICEMEM_INVALID_LMA_HEAP;
			goto ErrorPhysHeapsRelease;
		}

		/* Allocate memory for RA pointers and name strings */
		psDeviceNode->apsLocalDevMemArenas = OSAllocMem(sizeof(RA_ARENA*) * ui32NumOfLMARegions);
		if (!psDeviceNode->apsLocalDevMemArenas)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to allocate LocalDevMemArenas",
					 __func__));
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto ErrorPhysHeapsRelease;
		}
		psDeviceNode->ui32NumOfLocalMemArenas = ui32NumOfLMARegions;
		psDeviceNode->apszRANames = OSAllocMem(ui32NumOfLMARegions * sizeof(IMG_PCHAR));
		if (!psDeviceNode->apszRANames)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to allocate RANames",
					 __func__));
			OSFreeMem(psDeviceNode->apsLocalDevMemArenas);
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto ErrorPhysHeapsRelease;
		}

		for (; ui32RegionId < ui32NumOfLMARegions; ui32RegionId++)
		{
			eError = PhysHeapRegionGetSize(psLMAHeap, ui32RegionId, &ui64Size);
			if (eError != PVRSRV_OK)
			{
				/* We can only get here if there is a bug in this module */
				PVR_ASSERT(IMG_FALSE);
				return eError;
			}

			eError = PhysHeapRegionGetCpuPAddr(psLMAHeap, ui32RegionId, &sCpuPAddr);
			if (eError != PVRSRV_OK)
			{
				/* We can only get here if there is a bug in this module */
				PVR_ASSERT(IMG_FALSE);
				return eError;
			}

			eError = PhysHeapRegionGetDevPAddr(psLMAHeap, ui32RegionId, &sDevPAddr);
			if (eError != PVRSRV_OK)
			{
				/* We can only get here if there is a bug in this module */
				PVR_ASSERT(IMG_FALSE);
				return eError;
			}

			PVR_DPF((PVR_DBG_MESSAGE,
					"Creating RA for card memory - region %d - 0x%016"
					IMG_UINT64_FMTSPECx"-0x%016" IMG_UINT64_FMTSPECx,
					 ui32RegionId, (IMG_UINT64) sCpuPAddr.uiAddr,
					 sCpuPAddr.uiAddr + ui64Size));

			psDeviceNode->apszRANames[ui32RegionId] =
				OSAllocMem(PVRSRV_MAX_RA_NAME_LENGTH);
			if (!psDeviceNode->apszRANames[ui32RegionId])
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to alloc RANames[]",
						 __func__));
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto ErrorRAsDelete;
			}
			OSSNPrintf(psDeviceNode->apszRANames[ui32RegionId],
					   PVRSRV_MAX_RA_NAME_LENGTH,
					   "%s card mem",
					   psDevConfig->pszName);

			uBase = sDevPAddr.uiAddr;
			uSize = (RA_LENGTH_T) ui64Size;
			PVR_ASSERT(uSize == ui64Size);

			/* Use host page size, keeps things simple */
			psDeviceNode->apsLocalDevMemArenas[ui32RegionId] =
				RA_Create(psDeviceNode->apszRANames[ui32RegionId],
						  OSGetPageShift(), RA_LOCKCLASS_0, NULL, NULL, NULL,
						  IMG_FALSE);

			if (psDeviceNode->apsLocalDevMemArenas[ui32RegionId] == NULL)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create LMA memory arena",
						 __func__));
				OSFreeMem(psDeviceNode->apszRANames[ui32RegionId]);
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto ErrorRAsDelete;
			}

			if (!RA_Add(psDeviceNode->apsLocalDevMemArenas[ui32RegionId],
						uBase, uSize, 0, NULL))
			{
				PVR_DPF((PVR_DBG_ERROR,
						 "%s: Failed to add memory to LMA memory arena",
						 __func__));
				RA_Delete(psDeviceNode->apsLocalDevMemArenas[ui32RegionId]);
				OSFreeMem(psDeviceNode->apszRANames[ui32RegionId]);
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto ErrorRAsDelete;
			}
		}

#if defined(SUPPORT_GPUVIRT_VALIDATION)
		eError = CreateLMASubArenas(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to create LMA memory sub-arenas", __func__));
			goto ErrorRAsDelete;
		}
#endif

		/* If additional psDeviceNode->pfnDevPx* callbacks are added,
		   update the corresponding virtualization-specific override
		   in pvrsrv_vz.c:_VzDeviceCreate() */
		psDeviceNode->pfnDevPxAlloc = LMA_PhyContigPagesAlloc;
		psDeviceNode->pfnDevPxFree = LMA_PhyContigPagesFree;
		psDeviceNode->pfnDevPxMap = LMA_PhyContigPagesMap;
		psDeviceNode->pfnDevPxUnMap = LMA_PhyContigPagesUnmap;
		psDeviceNode->pfnDevPxClean = LMA_PhyContigPagesClean;
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL] = PhysmemNewLocalRamBackedPMR;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "===== OS System memory only, no local card memory"));

		/* else we only have OS system memory */
		psDeviceNode->pfnDevPxAlloc = OSPhyContigPagesAlloc;
		psDeviceNode->pfnDevPxFree = OSPhyContigPagesFree;
		psDeviceNode->pfnDevPxMap = OSPhyContigPagesMap;
		psDeviceNode->pfnDevPxUnMap = OSPhyContigPagesUnmap;
		psDeviceNode->pfnDevPxClean = OSPhyContigPagesClean;
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL] = PhysmemNewOSRamBackedPMR;
	}

	if (PhysHeapGetType(psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL]) == PHYS_HEAP_TYPE_LMA)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "===== Local card memory only, no OS system memory"));
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL] = PhysmemNewLocalRamBackedPMR;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "===== OS System memory, 2nd phys heap"));
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL] = PhysmemNewOSRamBackedPMR;
	}

	if (PhysHeapGetType(psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL]) == PHYS_HEAP_TYPE_LMA)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "===== Local card memory only, no OS system memory"));
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL] = PhysmemNewLocalRamBackedPMR;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "===== OS System memory, 3rd phys heap"));
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL] = PhysmemNewOSRamBackedPMR;
	}

	return PVRSRV_OK;

ErrorRAsDelete:
	while (ui32RegionId)
	{
		ui32RegionId--;
		RA_Delete(psDeviceNode->apsLocalDevMemArenas[ui32RegionId]);
		psDeviceNode->apsLocalDevMemArenas[ui32RegionId] = NULL;

		OSFreeMem(psDeviceNode->apszRANames[ui32RegionId]);
		psDeviceNode->apszRANames[ui32RegionId] = NULL;
	}

	OSFreeMem(psDeviceNode->apsLocalDevMemArenas);
	psDeviceNode->apsLocalDevMemArenas = NULL;

	OSFreeMem(psDeviceNode->apszRANames);
	psDeviceNode->apszRANames = NULL;

ErrorPhysHeapsRelease:
	for (physHeapIndex = 0;
		 physHeapIndex < ARRAY_SIZE(psDeviceNode->apsPhysHeap);
		 physHeapIndex++)
	{
		if (psDeviceNode->apsPhysHeap[physHeapIndex])
		{
			PhysHeapRelease(psDeviceNode->apsPhysHeap[physHeapIndex]);
		}
	}

ErrorPhysHeapsUnregister:
	for (i = 0; i < psDeviceNode->ui32RegisteredPhysHeaps; i++)
	{
		PhysHeapUnregister(psDeviceNode->papsRegisteredPhysHeaps[i]);
	}
	OSFreeMem(psDeviceNode->papsRegisteredPhysHeaps);

	return eError;
}

void PVRSRVPhysMemHeapsDeinit(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_DEVICE_PHYS_HEAP ePhysHeapIdx;
	IMG_UINT32 i;
	IMG_UINT32 ui32RegionIdx;

	/* Remove RAs and RA names for local card memory */
	for (ui32RegionIdx = 0;
		 ui32RegionIdx < psDeviceNode->ui32NumOfLocalMemArenas;
		 ui32RegionIdx++)
	{
		if (psDeviceNode->apsLocalDevMemArenas[ui32RegionIdx])
		{
			RA_Delete(psDeviceNode->apsLocalDevMemArenas[ui32RegionIdx]);
			psDeviceNode->apsLocalDevMemArenas[ui32RegionIdx] = NULL;
		}

		if (psDeviceNode->apszRANames[ui32RegionIdx])
		{
			OSFreeMem(psDeviceNode->apszRANames[ui32RegionIdx]);
			psDeviceNode->apszRANames[ui32RegionIdx] = NULL;
		}
	}

	if (psDeviceNode->apsLocalDevMemArenas)
	{
		OSFreeMem(psDeviceNode->apsLocalDevMemArenas);
		psDeviceNode->apsLocalDevMemArenas = NULL;
	}

	if (psDeviceNode->apszRANames)
	{
		OSFreeMem(psDeviceNode->apszRANames);
		psDeviceNode->apszRANames = NULL;
	}

	/* Release heaps */
	for (ePhysHeapIdx = 0;
		 ePhysHeapIdx < ARRAY_SIZE(psDeviceNode->apsPhysHeap);
		 ePhysHeapIdx++)
	{
		if (psDeviceNode->apsPhysHeap[ePhysHeapIdx])
		{
			PhysHeapRelease(psDeviceNode->apsPhysHeap[ePhysHeapIdx]);
		}
	}

	/* Unregister heaps */
	for (i = 0; i < psDeviceNode->ui32RegisteredPhysHeaps; i++)
	{
		PhysHeapUnregister(psDeviceNode->papsRegisteredPhysHeaps[i]);
	}

	OSFreeMem(psDeviceNode->papsRegisteredPhysHeaps);
}

PVRSRV_ERROR IMG_CALLCONV PVRSRVDeviceCreate(void *pvOSDevice,
											 IMG_INT32 i32UMIdentifier,
											 PVRSRV_DEVICE_NODE **ppsDeviceNode)
{
	PVRSRV_DATA				*psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR			eError;
	PVRSRV_DEVICE_CONFIG	*psDevConfig;
	PVRSRV_DEVICE_NODE		*psDeviceNode;
#if defined(SUPPORT_RGX) || defined(SUPPORT_ALT_REGBASE)
	PVRSRV_RGXDEV_INFO		*psDevInfo = NULL;
#endif
	IMG_UINT32				ui32AppHintDefault;
	IMG_UINT32				ui32AppHintDriverMode;
#if defined(SUPPORT_PHYSMEM_TEST) && !defined(INTEGRITY_OS) && !defined(__QNXNTO__)
	IMG_UINT32				ui32AppHintPhysMemTestPasses;
#endif
	void *pvAppHintState    = NULL;
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	IMG_HANDLE				hProcessStats;
#endif

	psDeviceNode = OSAllocZMemNoStats(sizeof(*psDeviceNode));
	if (!psDeviceNode)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to allocate device node",
				 __func__));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	/* Allocate process statistics */
	eError = PVRSRVStatsRegisterProcess(&hProcessStats);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Couldn't register process statistics (%d)",
			 __func__, eError));
		goto ErrorFreeDeviceNode;
	}
#endif

	psDeviceNode->sDevId.i32UMIdentifier = i32UMIdentifier;

	/* Read driver mode (i.e. native, host or guest) AppHint early */
	ui32AppHintDefault = PVRSRV_APPHINT_DRIVERMODE;
	OSCreateKMAppHintState(&pvAppHintState);
	OSGetKMAppHintUINT32(pvAppHintState, DriverMode,
						 &ui32AppHintDefault, &ui32AppHintDriverMode);
	OSFreeKMAppHintState(pvAppHintState);
	pvAppHintState = NULL;
	psPVRSRVData->eDriverMode = PVRSRV_VZ_APPHINT_MODE(ui32AppHintDriverMode);

	eError = SysDevInit(pvOSDevice, &psDevConfig);
	if (eError)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to get device config (%s)",
				 __func__, PVRSRVGetErrorString(eError)));
		goto ErrorDeregisterStats;
	}

	PVR_ASSERT(psDevConfig);
	PVR_ASSERT(psDevConfig->pvOSDevice == pvOSDevice);
	PVR_ASSERT(!psDevConfig->psDevNode);

	psDeviceNode->eDevState = PVRSRV_DEVICE_STATE_INIT;
	psDeviceNode->psDevConfig = psDevConfig;
	psDeviceNode->eCurrentSysPowerState = PVRSRV_SYS_POWER_STATE_ON;

	if (psDeviceNode->psDevConfig->pfnSysDriverMode)
	{
		if (! PVRSRV_VZ_APPHINT_MODE_IS_OVERRIDE(ui32AppHintDriverMode))
		{
			/*
			 * The driver mode AppHint can be an override and non-override (default)
			 * value. If the system layer provides a callback in SysDevInit() to
			 * force the driver into a particular driver mode, then only comply
			 * if the apphint value provided is a non-override mode value.
			 */
			psPVRSRVData->eDriverMode = psDeviceNode->psDevConfig->pfnSysDriverMode();
		}
		else
		{
			PVR_DPF((PVR_DBG_WARNING, "Override driver mode specified, ignoring SysDriveMode"));
		}
	}

	/*
	 * Ensure that the supplied driver execution mode is consistent with the number
	 * of OSIDs the firmware can support. Any failure here is (should be) fatal as
	 * the requested for driver mode cannot be supported by the firmware.
	 */
	switch (psPVRSRVData->eDriverMode)
	{
		case DRIVER_MODE_NATIVE:
		/* Always supported mode */
			break;

		case DRIVER_MODE_HOST:
		case DRIVER_MODE_GUEST:
#if (RGXFW_NUM_OS == 1)
			PVR_DPF((PVR_DBG_ERROR, "The number of firmware supported OSID(s) is 1"));
			PVR_DPF((PVR_DBG_ERROR,	"Halting initialisation, cannot transition to %s mode",
					psPVRSRVData->eDriverMode == DRIVER_MODE_HOST ? "host" : "guest"));
			eError = PVRSRV_ERROR_NOT_SUPPORTED;
			goto ErrorSysDevDeInit;
#endif
			break;

		default:
			if ((IMG_INT32)psPVRSRVData->eDriverMode < (IMG_INT32)DRIVER_MODE_NATIVE ||
			    (IMG_INT32)psPVRSRVData->eDriverMode >= (IMG_INT32)RGXFW_NUM_OS)
			{
				/* Running on non-VZ capable BVNC so simulating OSID using eDriverMode but
				   value is outside of permitted range */
				PVR_DPF((PVR_DBG_ERROR,
						"Halting initialisation, OSID %d is outside of range [0:%d] supported",
						(IMG_INT)psPVRSRVData->eDriverMode, RGXFW_NUM_OS-1));
				eError = PVRSRV_ERROR_NOT_SUPPORTED;
				goto ErrorSysDevDeInit;
			}
			else
			{
				/* Invalid driver mode enumeration integer value */
				PVR_DPF((PVR_DBG_ERROR, "Halting initialisation due to invalid driver mode %d",
						(IMG_INT32)psPVRSRVData->eDriverMode));
				eError = PVRSRV_ERROR_NOT_SUPPORTED;
				goto ErrorSysDevDeInit;
			}
			break;
	}

#if defined(SUPPORT_PHYSMEM_TEST) && !defined(INTEGRITY_OS) && !defined(__QNXNTO__)
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_NATIVE))
	{
		/* Read AppHint - Configurable memory test pass count */
		ui32AppHintDefault = 0;
		OSCreateKMAppHintState(&pvAppHintState);
		OSGetKMAppHintUINT32(pvAppHintState, PhysMemTestPasses,
				&ui32AppHintDefault, &ui32AppHintPhysMemTestPasses);
		OSFreeKMAppHintState(pvAppHintState);
		pvAppHintState = NULL;

		if(ui32AppHintPhysMemTestPasses > 0)
		{
			eError = PhysMemTest(psDevConfig, ui32AppHintPhysMemTestPasses);
			PVR_LOGG_IF_ERROR(eError, "PhysMemTest", ErrorSysDevDeInit);
		}
	}
#endif
	/* Store the device node in the device config for the system layer to use */
	psDevConfig->psDevNode = psDeviceNode;

	/* Perform additional VZ system initialisation */
	eError = SysVzDevInit(psDevConfig);
	if (eError)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed system virtualization initialisation (%s)",
				 __func__, PVRSRVGetErrorString(eError)));
		goto ErrorSysDevDeInit;
	}

	eError = PVRSRVRegisterDbgTable(psDeviceNode,
									g_aui32DebugOrderTable,
									ARRAY_SIZE(g_aui32DebugOrderTable));
	if (eError != PVRSRV_OK)
	{
		goto ErrorSysVzDevDeInit;
	}

	eError = OSLockCreate(&psDeviceNode->hPowerLock);
	if (eError != PVRSRV_OK)
	{
		goto ErrorUnregisterDbgTable;
	}

	eError = PVRSRVPhysMemHeapsInit(psDeviceNode, psDevConfig);
	if (eError != PVRSRV_OK)
	{
		goto ErrorPowerLockDestroy;
	}

#if defined(SUPPORT_RGX)
	/* Requires registered GPU local heap */
	/* Requires debug table */
	/* Initialises psDevInfo */
	eError = RGXRegisterDevice(psDeviceNode, &psDevInfo);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to register device", __func__));
		eError = PVRSRV_ERROR_DEVICE_REGISTER_FAILED;
		goto ErrorPhysMemHeapsDeinit;
	}
#endif

	psDeviceNode->uiMMUPxLog2AllocGran = OSGetPageShift();

	eError = ServerSyncInit(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		goto ErrorDeInitRgx;
	}

	eError = SyncCheckpointInit(psDeviceNode);
	PVR_LOG_IF_ERROR(eError, "SyncCheckpointInit");

#if defined(SUPPORT_RGX) && defined(SUPPORT_DEDICATED_FW_MEMORY) && !defined(NO_HARDWARE)
	eError = PhysmemInitFWDedicatedMem(psDeviceNode, psDevConfig);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to initialise dedicated FW memory heap",
				 __func__));
		goto ErrorOnFWMemInit;
	}
#endif

	/* Perform additional vz initialisation */
	eError = _VzDeviceCreate(psDeviceNode);
	PVR_LOG_IF_ERROR(eError, "_VzDeviceCreate");

	/*
	 * This is registered before doing device specific initialisation to ensure
	 * generic device information is dumped first during a debug request.
	 */
	eError = PVRSRVRegisterDbgRequestNotify(&psDeviceNode->hDbgReqNotify,
											psDeviceNode,
											_SysDebugRequestNotify,
											DEBUG_REQUEST_SYS,
											psDeviceNode);
	PVR_LOG_IF_ERROR(eError, "PVRSRVRegisterDbgRequestNotify");

	eError = PVRSRVRegisterDbgRequestNotify(&psDeviceNode->hThreadsDbgReqNotify,
												psDeviceNode,
												_ThreadsDebugRequestNotify,
												DEBUG_REQUEST_SYS,
												NULL);
	PVR_LOG_IF_ERROR(eError, "PVRSRVRegisterDbgRequestNotify");

	eError = HTBDeviceCreate(psDeviceNode);
	PVR_LOG_IF_ERROR(eError, "HTBDeviceCreate");

	psPVRSRVData->ui32RegisteredDevices++;

#if defined(PVR_DVFS) && !defined(NO_HARDWARE)
	eError = InitDVFS(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to start DVFS", __func__));
		goto ErrorDecrementDeviceCount;
	}
#endif

	OSAtomicWrite(&psDeviceNode->iNumClockSpeedChanges, 0);

#if defined(PVR_TESTING_UTILS)
	TUtilsInit(psDeviceNode);
#endif

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSWRLockCreate(&psDeviceNode->hMemoryContextPageFaultNotifyListLock);
	if (psDeviceNode->hMemoryContextPageFaultNotifyListLock == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create lock for PF notify list",
		        __func__));
		goto ErrorDecrementDeviceCount;
	}
#endif /* !defined(PVRSRV_USE_BRIDGE_LOCK) */

	dllist_init(&psDeviceNode->sMemoryContextPageFaultNotifyListHead);

	PVR_DPF((PVR_DBG_MESSAGE, "Registered device %p", psDeviceNode));
	PVR_DPF((PVR_DBG_MESSAGE, "Register bank address = 0x%08lx",
			 (unsigned long)psDevConfig->sRegsCpuPBase.uiAddr));
	PVR_DPF((PVR_DBG_MESSAGE, "IRQ = %d", psDevConfig->ui32IRQ));

#if defined(SUPPORT_RGX) && defined(SUPPORT_ALT_REGBASE)
	{
		IMG_DEV_PHYADDR sRegsGpuPBase;

		PhysHeapCpuPAddrToDevPAddr(psDevInfo->psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL],
		                           1,
		                           &sRegsGpuPBase,
		                           &(psDeviceNode->psDevConfig->sRegsCpuPBase));

		PVR_LOG(("%s: Using alternate Register bank GPU address: 0x%08lx (orig: 0x%08lx)", __func__,
		         (unsigned long)psDevConfig->sAltRegsGpuPBase.uiAddr,
		         (unsigned long)sRegsGpuPBase.uiAddr));
	}
#endif

	/* Finally insert the device into the dev-list and set it as active */
	List_PVRSRV_DEVICE_NODE_InsertTail(&psPVRSRVData->psDeviceNodeList,
									   psDeviceNode);

	*ppsDeviceNode = psDeviceNode;

#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	/* Close the process statistics */
	PVRSRVStatsDeregisterProcess(hProcessStats);
#endif

#if defined(SUPPORT_VALIDATION) && !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockCreateNoStats(&psDeviceNode->hValidationLock);
#endif

	return PVRSRV_OK;

#if (defined(PVR_DVFS) && !defined(NO_HARDWARE)) || !defined(PVRSRV_USE_BRIDGE_LOCK)
ErrorDecrementDeviceCount:
#endif
	psPVRSRVData->ui32RegisteredDevices--;

	if (psDeviceNode->hDbgReqNotify)
	{
		PVRSRVUnregisterDbgRequestNotify(psDeviceNode->hDbgReqNotify);
	}

	if (psDeviceNode->hThreadsDbgReqNotify)
	{
		PVRSRVUnregisterDbgRequestNotify(psDeviceNode->hThreadsDbgReqNotify);
	}

	/* Perform vz deinitialisation */
	_VzDeviceDestroy(psDeviceNode);

#if defined(SUPPORT_RGX) && defined(SUPPORT_DEDICATED_FW_MEMORY) && !defined(NO_HARDWARE)
ErrorOnFWMemInit:
	PhysmemDeinitFWDedicatedMem(psDeviceNode);
#endif

	ServerSyncDeinit(psDeviceNode);

ErrorDeInitRgx:
#if defined(SUPPORT_RGX)
	DevDeInitRGX(psDeviceNode);
ErrorPhysMemHeapsDeinit:
	PVRSRVPhysMemHeapsDeinit(psDeviceNode);
#endif
ErrorPowerLockDestroy:
	OSLockDestroy(psDeviceNode->hPowerLock);
ErrorUnregisterDbgTable:
	PVRSRVUnregisterDbgTable(psDeviceNode);
ErrorSysVzDevDeInit:
	psDevConfig->psDevNode = NULL;
	SysVzDevDeInit(psDevConfig);
ErrorSysDevDeInit:
	SysDevDeInit(psDevConfig);
ErrorDeregisterStats:
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	/* Close the process statistics */
	PVRSRVStatsDeregisterProcess(hProcessStats);
ErrorFreeDeviceNode:
#endif
	OSFreeMemNoStats(psDeviceNode);

	return eError;
}

#if defined(SUPPORT_RGX)
static PVRSRV_ERROR _SetDeviceFlag(const PVRSRV_DEVICE_NODE *psDevice,
                                   const void *psPrivate, IMG_BOOL bValue)
{
	PVRSRV_ERROR eResult = PVRSRV_OK;
	IMG_UINT32 ui32Flag = (IMG_UINT32)((uintptr_t)psPrivate);

	if (!ui32Flag)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eResult = RGXSetDeviceFlags((PVRSRV_RGXDEV_INFO *)psDevice->pvDevice,
	                            ui32Flag, bValue);

	return eResult;
}

static PVRSRV_ERROR _ReadDeviceFlag(const PVRSRV_DEVICE_NODE *psDevice,
                                   const void *psPrivate, IMG_BOOL *pbValue)
{
	PVRSRV_ERROR eResult = PVRSRV_OK;
	IMG_UINT32 ui32Flag = (IMG_UINT32)((uintptr_t)psPrivate);
	IMG_UINT32 ui32State;

	if (!ui32Flag)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eResult = RGXGetDeviceFlags((PVRSRV_RGXDEV_INFO *)psDevice->pvDevice,
	                            &ui32State);

	if (PVRSRV_OK == eResult)
	{
		*pbValue = (ui32State & ui32Flag)? IMG_TRUE: IMG_FALSE;
	}

	return eResult;
}
static PVRSRV_ERROR _SetStateFlag(const PVRSRV_DEVICE_NODE *psDevice,
                                  const void *psPrivate, IMG_BOOL bValue)
{
	PVRSRV_ERROR eResult = PVRSRV_OK;
	IMG_UINT32 ui32Flag = (IMG_UINT32)((uintptr_t)psPrivate);

	if (!ui32Flag)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* EnableHWR is a special case
	 * only possible to disable after FW is running
	 */
	if (bValue && RGXFWIF_INICFG_HWR_EN == ui32Flag)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	eResult = RGXStateFlagCtrl((PVRSRV_RGXDEV_INFO *)psDevice->pvDevice,
	                           ui32Flag, NULL, bValue);

	return eResult;
}

static PVRSRV_ERROR _ReadStateFlag(const PVRSRV_DEVICE_NODE *psDevice,
                                   const void *psPrivate, IMG_BOOL *pbValue)
{
	IMG_UINT32 ui32Flag = (IMG_UINT32)((uintptr_t)psPrivate);
	IMG_UINT32 ui32State;
	PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO *)psDevice->pvDevice;

	if (!ui32Flag)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	ui32State = psDevInfo->psFWIfOSConfig->ui32ConfigFlags;

	if (pbValue)
	{
		*pbValue = (ui32State & ui32Flag)? IMG_TRUE: IMG_FALSE;
	}

	return PVRSRV_OK;
}
#endif

PVRSRV_ERROR PVRSRVDeviceInitialise(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	IMG_BOOL bInitSuccesful = IMG_FALSE;
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	IMG_HANDLE hProcessStats;
#endif
	PVRSRV_ERROR eError;

	if (psDeviceNode->eDevState != PVRSRV_DEVICE_STATE_INIT)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Device already initialised", __func__));
		return PVRSRV_ERROR_INIT_FAILURE;
	}

	/* Allocate process statistics */
#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	eError = PVRSRVStatsRegisterProcess(&hProcessStats);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Couldn't register process statistics (%d)",
			 __func__, eError));
		return eError;
	}
#endif

#if defined(SUPPORT_RGX)
	eError = RGXInit(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Initialisation of Rogue device failed (%s)",
				 __func__, PVRSRVGetErrorString(eError)));
		goto Exit;
	}
#endif

	bInitSuccesful = IMG_TRUE;

#if defined(SUPPORT_RGX)
Exit:
#endif
	eError = PVRSRVDeviceFinalise(psDeviceNode, bInitSuccesful);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Services failed to finalise the device (%s)",
				 __func__, PVRSRVGetErrorString(eError)));
	}

#if defined(SUPPORT_RGX)
	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_DisableClockGating,
	                                  _ReadStateFlag, _SetStateFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXFWIF_INICFG_DISABLE_CLKGATING_EN));
	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_DisableDMOverlap,
	                                  _ReadStateFlag, _SetStateFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXFWIF_INICFG_DISABLE_DM_OVERLAP));
	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_AssertOnHWRTrigger,
	                                  _ReadStateFlag, _SetStateFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXFWIF_INICFG_ASSERT_ON_HWR_TRIGGER));
	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_AssertOutOfMemory,
	                                  _ReadStateFlag, _SetStateFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXFWIF_INICFG_ASSERT_ON_OUTOFMEMORY));
	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_CheckMList,
	                                  _ReadStateFlag, _SetStateFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXFWIF_INICFG_CHECK_MLIST_EN));
	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_EnableHWR,
	                                  _ReadStateFlag, _SetStateFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXFWIF_INICFG_HWR_EN));

	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_DisableFEDLogging,
	                                  _ReadDeviceFlag, _SetDeviceFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXKM_DEVICE_STATE_DISABLE_DW_LOGGING_EN));
	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_ZeroFreelist,
	                                  _ReadDeviceFlag, _SetDeviceFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXKM_DEVICE_STATE_ZERO_FREELIST));
	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_DustRequestInject,
	                                  _ReadDeviceFlag, _SetDeviceFlag,
	                                  psDeviceNode,
	                                  (void*)((uintptr_t)RGXKM_DEVICE_STATE_DUST_REQUEST_INJECT_EN));

	PVRSRVAppHintRegisterHandlersBOOL(APPHINT_ID_DisablePDumpPanic,
	                                  RGXQueryPdumpPanicDisable, RGXSetPdumpPanicDisable,
	                                  psDeviceNode,
	                                  NULL);
#endif

#if defined(PVRSRV_ENABLE_PROCESS_STATS) && !defined(PVRSRV_DEBUG_LINUX_MEMORY_STATS)
	/* Close the process statistics */
	PVRSRVStatsDeregisterProcess(hProcessStats);
#endif

	return eError;
}

PVRSRV_ERROR IMG_CALLCONV PVRSRVDeviceDestroy(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_DATA				*psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_ERROR			eError;
#if defined(PVRSRV_FORCE_UNLOAD_IF_BAD_STATE)
	IMG_BOOL				bForceUnload = IMG_FALSE;

	if (PVRSRVGetPVRSRVData()->eServicesState != PVRSRV_SERVICES_STATE_OK)
	{
		bForceUnload = IMG_TRUE;
	}
#endif

	psPVRSRVData->ui32RegisteredDevices--;

	psDeviceNode->eDevState = PVRSRV_DEVICE_STATE_DEINIT;

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	if (psDeviceNode->hMemoryContextPageFaultNotifyListLock != NULL)
	{
		OSWRLockDestroy(psDeviceNode->hMemoryContextPageFaultNotifyListLock);
	}
#endif /* !defined(PVRSRV_USE_BRIDGE_LOCK) */

#if defined(SUPPORT_VALIDATION) && !defined(PVRSRV_USE_BRIDGE_LOCK)
	OSLockDestroyNoStats(psDeviceNode->hValidationLock);
	psDeviceNode->hValidationLock = NULL;
#endif

#if defined(PVR_TESTING_UTILS)
	TUtilsDeinit(psDeviceNode);
#endif
#if defined(SUPPORT_FALLBACK_FENCE_SYNC)
	SyncFbDeregisterDevice(psDeviceNode);
#endif
	/* Counter part to what gets done in PVRSRVDeviceFinalise */
	if (psDeviceNode->hSyncCheckpointContext)
	{
		SyncCheckpointContextDestroy(psDeviceNode->hSyncCheckpointContext);
		psDeviceNode->hSyncCheckpointContext = NULL;
	}
	if (psDeviceNode->hSyncPrimContext)
	{
		if (psDeviceNode->psSyncPrim)
		{
			/* Free general purpose sync primitive */
			SyncPrimFree(psDeviceNode->psSyncPrim);
			psDeviceNode->psSyncPrim = NULL;
		}

		if (psDeviceNode->psMMUCacheSyncPrim)
		{
			PVRSRV_CLIENT_SYNC_PRIM *psSync = psDeviceNode->psMMUCacheSyncPrim;

			/* Ensure there are no pending MMU Cache Ops in progress before freeing this sync. */
			eError = PVRSRVPollForValueKM(psSync->pui32LinAddr,
			                              psDeviceNode->ui16NextMMUInvalidateUpdate-1,
			                              0xFFFFFFFF);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to wait for MMU Cache op", __func__));
				return eError;
			}

			/* Important to set the device node pointer to NULL
			 * before we free the sync-prim to make sure we don't
			 * defer the freeing of the sync-prim's page tables itself.
			 * The sync is used to defer the MMU page table
			 * freeing. */
			psDeviceNode->psMMUCacheSyncPrim = NULL;

			/* Free general purpose sync primitive */
			SyncPrimFree(psSync);
		}

		SyncPrimContextDestroy(psDeviceNode->hSyncPrimContext);
		psDeviceNode->hSyncPrimContext = NULL;
	}

	eError = PVRSRVPowerLock(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to acquire power lock", __func__));
		return eError;
	}

#if defined(PVRSRV_FORCE_UNLOAD_IF_BAD_STATE)
	/*
	 * Firmware probably not responding if bForceUnload is set, but we still want to unload the
	 * driver.
	 */
	if (!bForceUnload)
#endif
	{
		/* Force idle device */
		eError = PVRSRVDeviceIdleRequestKM(psDeviceNode, NULL, IMG_TRUE);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Forced idle request failure (%s)",
			                        __func__, PVRSRVGetErrorString(eError)));
			if (eError != PVRSRV_ERROR_PWLOCK_RELEASED_REACQ_FAILED)
			{
				PVRSRVPowerUnlock(psDeviceNode);
			}
			return eError;
		}
	}

	/* Power down the device if necessary */
	eError = PVRSRVSetDevicePowerStateKM(psDeviceNode,
										 PVRSRV_DEV_POWER_STATE_OFF,
										 IMG_TRUE);
	PVRSRVPowerUnlock(psDeviceNode);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed PVRSRVSetDevicePowerStateKM call (%s). Dump debug.",
				 __func__, PVRSRVGetErrorString(eError)));

		PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);

		/*
		 * If the driver is okay then return the error, otherwise we can ignore
		 * this error.
		 */
		if (PVRSRVGetPVRSRVData()->eServicesState == PVRSRV_SERVICES_STATE_OK)
		{
			return eError;
		}
		else
		{
			PVR_DPF((PVR_DBG_MESSAGE,
					 "%s: Will continue to unregister as driver status is not OK",
					 __func__));
		}
	}

#if defined(SUPPORT_RGX)
	DevDeInitRGX(psDeviceNode);
#endif

	HTBDeviceDestroy(psDeviceNode);

	if (psDeviceNode->hDbgReqNotify)
	{
		PVRSRVUnregisterDbgRequestNotify(psDeviceNode->hDbgReqNotify);
	}

	if (psDeviceNode->hThreadsDbgReqNotify)
	{
		PVRSRVUnregisterDbgRequestNotify(psDeviceNode->hThreadsDbgReqNotify);
	}

	SyncCheckpointDeinit(psDeviceNode);

#if defined(SUPPORT_SERVER_SYNC_IMPL)
	ServerSyncDeinit(psDeviceNode);
#endif
#if defined(SUPPORT_RGX) && defined(SUPPORT_DEDICATED_FW_MEMORY) && !defined(NO_HARDWARE)
	PhysmemDeinitFWDedicatedMem(psDeviceNode);
#endif

	/* Perform vz deinitialisation */
	_VzDeviceDestroy(psDeviceNode);

	List_PVRSRV_DEVICE_NODE_Remove(psDeviceNode);

#if defined(PVR_DVFS) && !defined(NO_HARDWARE)
	DeinitDVFS(psDeviceNode);
#endif

	PVRSRVPhysMemHeapsDeinit(psDeviceNode);
	OSLockDestroy(psDeviceNode->hPowerLock);

	PVRSRVUnregisterDbgTable(psDeviceNode);

	psDeviceNode->psDevConfig->psDevNode = NULL;
	SysVzDevDeInit(psDeviceNode->psDevConfig);
	SysDevDeInit(psDeviceNode->psDevConfig);

	OSFreeMemNoStats(psDeviceNode);

	return PVRSRV_OK;
}

PVRSRV_ERROR LMA_PhyContigPagesAlloc(PVRSRV_DEVICE_NODE *psDevNode, size_t uiSize,
							PG_HANDLE *psMemHandle, IMG_DEV_PHYADDR *psDevPAddr)
{
#if defined(SUPPORT_GPUVIRT_VALIDATION)
	IMG_UINT32 ui32OSid = 0;
#endif
	RA_BASE_T uiCardAddr;
	RA_LENGTH_T uiActualSize;
	PVRSRV_ERROR eError;

	RA_ARENA *pArena=psDevNode->apsLocalDevMemArenas[0];
	IMG_UINT32 ui32Log2NumPages = 0;

	PVR_ASSERT(uiSize != 0);
	ui32Log2NumPages = OSGetOrder(uiSize);
	uiSize = (1 << ui32Log2NumPages) * OSGetPageSize();

#if defined(SUPPORT_GPUVIRT_VALIDATION)
{
	IMG_UINT32 ui32OSidReg = 0;
	IMG_BOOL   bOSidAxiProt;

	IMG_PID    pId = OSGetCurrentClientProcessIDKM();

	RetrieveOSidsfromPidList(pId, &ui32OSid, &ui32OSidReg, &bOSidAxiProt);

	pArena = psDevNode->psOSidSubArena[ui32OSid];
}
#endif

	eError = RA_Alloc(pArena,
	                  uiSize,
	                  RA_NO_IMPORT_MULTIPLIER,
	                  0,                         /* No flags */
	                  uiSize,
	                  "LMA_PhyContigPagesAlloc",
	                  &uiCardAddr,
	                  &uiActualSize,
	                  NULL);                     /* No private handle */

	PVR_ASSERT(uiSize == uiActualSize);

#if defined(SUPPORT_GPUVIRT_VALIDATION)
{
	PVR_DPF((PVR_DBG_MESSAGE,"(GPU Virtualization Validation): LMA_PhyContigPagesAlloc: Address:%llu, size:%llu", uiCardAddr,uiActualSize));
}
#endif

	psMemHandle->u.ui64Handle = uiCardAddr;
	psDevPAddr->uiAddr = (IMG_UINT64) uiCardAddr;

	if (PVRSRV_OK == eError)
	{
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
	    PVRSRVStatsIncrMemAllocStatAndTrack(PVRSRV_MEM_ALLOC_TYPE_ALLOC_PAGES_PT_LMA,
	                                        uiSize,
	                                        (IMG_UINT64)(uintptr_t) psMemHandle,
		                                    OSGetCurrentClientProcessIDKM());
#else
		IMG_CPU_PHYADDR sCpuPAddr;
		sCpuPAddr.uiAddr = psDevPAddr->uiAddr;

		PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_ALLOC_PAGES_PT_LMA,
		                             NULL,
		                             sCpuPAddr,
		                             uiSize,
		                             NULL,
		                             OSGetCurrentClientProcessIDKM());
#endif
#endif
		psMemHandle->ui32Order = ui32Log2NumPages;
	}

	return eError;
}

void LMA_PhyContigPagesFree(PVRSRV_DEVICE_NODE *psDevNode, PG_HANDLE *psMemHandle)
{
	RA_BASE_T uiCardAddr = (RA_BASE_T) psMemHandle->u.ui64Handle;

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
	PVRSRVStatsDecrMemAllocStatAndUntrack(PVRSRV_MEM_ALLOC_TYPE_ALLOC_PAGES_PT_LMA,
	                                      (IMG_UINT64)(uintptr_t) psMemHandle);
#else
	PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_ALLOC_PAGES_PT_LMA,
									(IMG_UINT64)uiCardAddr,
									OSGetCurrentClientProcessIDKM());
#endif
#endif
	RA_Free(psDevNode->apsLocalDevMemArenas[0], uiCardAddr);
	psMemHandle->ui32Order = 0;
}

PVRSRV_ERROR LMA_PhyContigPagesMap(PVRSRV_DEVICE_NODE *psDevNode, PG_HANDLE *psMemHandle,
							size_t uiSize, IMG_DEV_PHYADDR *psDevPAddr,
							void **pvPtr)
{
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_UINT32 ui32NumPages = (1 << psMemHandle->ui32Order);
	PVR_UNREFERENCED_PARAMETER(psMemHandle);
	PVR_UNREFERENCED_PARAMETER(uiSize);

	PhysHeapDevPAddrToCpuPAddr(psDevNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL], 1, &sCpuPAddr, psDevPAddr);
	*pvPtr = OSMapPhysToLin(sCpuPAddr,
							ui32NumPages * OSGetPageSize(),
							PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE);
	if (*pvPtr == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	else
	{
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
		PVRSRVStatsIncrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_IOREMAP_PT_LMA,
		                            ui32NumPages * OSGetPageSize(),
		                            OSGetCurrentClientProcessIDKM());
#else
		{
			PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_IOREMAP_PT_LMA,
										 *pvPtr,
										 sCpuPAddr,
										 ui32NumPages * OSGetPageSize(),
										 NULL,
										 OSGetCurrentClientProcessIDKM());
		}
#endif
#endif
		return PVRSRV_OK;
	}
}

void LMA_PhyContigPagesUnmap(PVRSRV_DEVICE_NODE *psDevNode, PG_HANDLE *psMemHandle,
						void *pvPtr)
{
	IMG_UINT32 ui32NumPages = (1 << psMemHandle->ui32Order);
	PVR_UNREFERENCED_PARAMETER(psMemHandle);
	PVR_UNREFERENCED_PARAMETER(psDevNode);

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#if !defined(PVRSRV_ENABLE_MEMORY_STATS)
	PVRSRVStatsDecrMemAllocStat(PVRSRV_MEM_ALLOC_TYPE_IOREMAP_PT_LMA,
		                            ui32NumPages * OSGetPageSize(),
		                            OSGetCurrentClientProcessIDKM());
#else
	PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE_IOREMAP_PT_LMA,
	                                (IMG_UINT64)(uintptr_t)pvPtr,
	                                OSGetCurrentClientProcessIDKM());
#endif
#endif

	OSUnMapPhysToLin(pvPtr, ui32NumPages * OSGetPageSize(),
					 PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);
}

PVRSRV_ERROR LMA_PhyContigPagesClean(PVRSRV_DEVICE_NODE *psDevNode,
                                     PG_HANDLE *psMemHandle,
                                     IMG_UINT32 uiOffset,
                                     IMG_UINT32 uiLength)
{
	/* No need to flush because we map as uncached */
	PVR_UNREFERENCED_PARAMETER(psDevNode);
	PVR_UNREFERENCED_PARAMETER(psMemHandle);
	PVR_UNREFERENCED_PARAMETER(uiOffset);
	PVR_UNREFERENCED_PARAMETER(uiLength);

	return PVRSRV_OK;
}

IMG_BOOL IsPhysmemNewRamBackedByLMA(PVRSRV_DEVICE_NODE *psDeviceNode, PVRSRV_DEVICE_PHYS_HEAP ePhysHeapIdx)
{
	return psDeviceNode->pfnCreateRamBackedPMR[ePhysHeapIdx] == PhysmemNewLocalRamBackedPMR;
}

/**************************************************************************/ /*!
@Function     PVRSRVDeviceFinalise
@Description  Performs the final parts of device initialisation.
@Input        psDeviceNode            Device node of the device to finish
                                      initialising
@Input        bInitSuccessful         Whether or not device specific
                                      initialisation was successful
@Return       PVRSRV_ERROR     PVRSRV_OK on success and an error otherwise
*/ /***************************************************************************/
PVRSRV_ERROR IMG_CALLCONV PVRSRVDeviceFinalise(PVRSRV_DEVICE_NODE *psDeviceNode,
											   IMG_BOOL bInitSuccessful)
{
	PVRSRV_ERROR eError;

	if (bInitSuccessful)
	{
		eError = SyncCheckpointContextCreate(psDeviceNode,
											 &psDeviceNode->hSyncCheckpointContext);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to create sync checkpoint context (%s)",
					 __func__, PVRSRVGetErrorString(eError)));

			goto ErrorExit;
		}
#if defined(SUPPORT_FALLBACK_FENCE_SYNC)
		eError = SyncFbRegisterDevice(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			goto ErrorExit;
		}
#endif
		eError = SyncPrimContextCreate(psDeviceNode,
									   &psDeviceNode->hSyncPrimContext);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to create sync prim context (%s)",
					 __func__, PVRSRVGetErrorString(eError)));
			SyncCheckpointContextDestroy(psDeviceNode->hSyncCheckpointContext);
			goto ErrorExit;
		}

		/* Allocate general purpose sync primitive */
		eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
							   &psDeviceNode->psSyncPrim,
							   "pvrsrv dev general");
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to allocate sync primitive with error (%s)",
					 __func__, PVRSRVGetErrorString(eError)));
			goto ErrorExit;
		}

		/* Allocate MMU cache invalidate sync */
		eError = SyncPrimAlloc(psDeviceNode->hSyncPrimContext,
							   &psDeviceNode->psMMUCacheSyncPrim,
							   "pvrsrv dev MMU cache");
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to allocate sync primitive with error (%s)",
					 __func__, PVRSRVGetErrorString(eError)));
			goto ErrorExit;
		}

		/* Next update value will be 1 since sync prim starts with 0 */
		psDeviceNode->ui16NextMMUInvalidateUpdate = 1;

		eError = PVRSRVPowerLock(psDeviceNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to acquire power lock (%s)",
					 __func__, PVRSRVGetErrorString(eError)));
			goto ErrorExit;
		}

		/*
		 * Always ensure a single power on command appears in the pdump. This
		 * should be the only power related call outside of PDUMPPOWCMDSTART
		 * and PDUMPPOWCMDEND.
		 */
		eError = PVRSRVSetDevicePowerStateKM(psDeviceNode,
											 PVRSRV_DEV_POWER_STATE_ON, IMG_TRUE);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to set device %p power state to 'on' (%s)",
					 __func__, psDeviceNode, PVRSRVGetErrorString(eError)));
			PVRSRVPowerUnlock(psDeviceNode);
			goto ErrorExit;
		}

#if defined(SUPPORT_EXTRA_METASP_DEBUG)
		eError = ValidateFWOnLoad(psDeviceNode->pvDevice);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to verify FW code (%s)",
					 __func__, PVRSRVGetErrorString(eError)));
			PVRSRVPowerUnlock(psDeviceNode);
			return eError;
		}
#endif

		/* Verify firmware compatibility for device */
		if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
		{
			/* defer the compatibility checks in case of Guest Mode until after
			 * the first kick was submitted, as the firmware only fills the
			 * compatibility data then.  */
			eError = PVRSRV_OK;
		}
		else
		{
			eError = PVRSRVDevInitCompatCheck(psDeviceNode);
		}

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed compatibility check for device %p (%s)",
					 __func__, psDeviceNode, PVRSRVGetErrorString(eError)));
			PVRSRVPowerUnlock(psDeviceNode);
			PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
			goto ErrorExit;
		}

		PDUMPPOWCMDSTART();

		/* Force the device to idle if its default power state is off */
		eError = PVRSRVDeviceIdleRequestKM(psDeviceNode,
										   &PVRSRVDeviceIsDefaultStateOFF,
										   IMG_TRUE);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Forced idle request failure (%s)",
			                        __func__, PVRSRVGetErrorString(eError)));
			if (eError != PVRSRV_ERROR_PWLOCK_RELEASED_REACQ_FAILED)
			{
				PVRSRVPowerUnlock(psDeviceNode);
			}
			goto ErrorExit;
		}

		/* Place device into its default power state. */
		eError = PVRSRVSetDevicePowerStateKM(psDeviceNode,
											 PVRSRV_DEV_POWER_STATE_DEFAULT,
											 IMG_TRUE);
		PDUMPPOWCMDEND();

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to set device %p into its default power state (%s)",
					 __func__, psDeviceNode, PVRSRVGetErrorString(eError)));

			PVRSRVPowerUnlock(psDeviceNode);
			goto ErrorExit;
		}

		PVRSRVPowerUnlock(psDeviceNode);

		/*
		 * If PDUMP is enabled and RGX device is supported, then initialise the
		 * performance counters that can be further modified in PDUMP. Then,
		 * before ending the init phase of the pdump, drain the commands put in
		 * the kCCB during the init phase.
		 */
#if defined(SUPPORT_RGX) && defined(PDUMP)
		{
			PVRSRV_RGXDEV_INFO *psDevInfo =
				(PVRSRV_RGXDEV_INFO *)(psDeviceNode->pvDevice);

			eError = RGXInitHWPerfCounters(psDeviceNode);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,
						 "%s: Failed to init hwperf counters (%s)",
						 __func__, PVRSRVGetErrorString(eError)));
				goto ErrorExit;
			}

			eError = RGXPdumpDrainKCCB(psDevInfo,
									   psDevInfo->psKernelCCBCtl->ui32WriteOffset);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Problem draining kCCB (%s)",
						 __func__, PVRSRVGetErrorString(eError)));
				goto ErrorExit;
			}
		}
#endif

		/* Now that the device(s) are fully initialised set them as active */
		psDeviceNode->eDevState = PVRSRV_DEVICE_STATE_ACTIVE;
		eError = PVRSRV_OK;

#if defined(SUPPORT_RGX)
		if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
		{
			/* Kick an initial dummy command to make the firmware initialise all
			 * its internal guest OS data structures and compatibility information */
			if (RGXFWHealthCheckCmd((PVRSRV_RGXDEV_INFO *)(psDeviceNode->pvDevice)) != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Cannot kick initial command to the Device (%s)",
						 __func__, PVRSRVGetErrorString(eError)));
				goto ErrorExit;
			}

			eError = PVRSRVDevInitCompatCheck(psDeviceNode);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,
						 "%s: Failed compatibility check for device %p (%s)",
						 __func__, psDeviceNode, PVRSRVGetErrorString(eError)));
				PVRSRVPowerUnlock(psDeviceNode);
				PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
				goto ErrorExit;
			}
		}
#endif
	}
	else
	{
		/* Initialisation failed so set the device(s) into a bad state */
		psDeviceNode->eDevState = PVRSRV_DEVICE_STATE_BAD;
		eError = PVRSRV_ERROR_NOT_INITIALISED;
	}

	/* Give PDump control a chance to end the init phase, depends on OS */
	PDumpStopInitPhase();

	return eError;

ErrorExit:
	/* Initialisation failed so set the device(s) into a bad state */
	psDeviceNode->eDevState = PVRSRV_DEVICE_STATE_BAD;

	return eError;
}

PVRSRV_ERROR IMG_CALLCONV PVRSRVDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	/* Only check devices which specify a compatibility check callback */
	if (psDeviceNode->pfnInitDeviceCompatCheck)
		return psDeviceNode->pfnInitDeviceCompatCheck(psDeviceNode);
	else
		return PVRSRV_OK;
}

#ifdef MTK_POWER_OFF_TIMING
/*
	PollForGEValueKM
*/
static
PVRSRV_ERROR IMG_CALLCONV PollForGEValueKM (volatile IMG_UINT32*	pui32LinMemAddr,
										  IMG_UINT32			ui32Value,
										  IMG_UINT32			ui32Mask,
										  IMG_UINT32			ui32Timeoutus,
										  IMG_UINT32			ui32PollPeriodus,
										  IMG_BOOL				bAllowPreemption)
{
#if defined(NO_HARDWARE)
	PVR_UNREFERENCED_PARAMETER(pui32LinMemAddr);
	PVR_UNREFERENCED_PARAMETER(ui32Value);
	PVR_UNREFERENCED_PARAMETER(ui32Mask);
	PVR_UNREFERENCED_PARAMETER(ui32Timeoutus);
	PVR_UNREFERENCED_PARAMETER(ui32PollPeriodus);
	PVR_UNREFERENCED_PARAMETER(bAllowPreemption);
	return PVRSRV_OK;
#else
	IMG_UINT32	ui32ActualValue = 0xFFFFFFFFU; /* Initialiser only required to prevent incorrect warning */
    IMG_UINT32	const ui32MaxValue = ui32Value+16;

	if (bAllowPreemption)
	{
		PVR_ASSERT(ui32PollPeriodus >= 1000);
	}

	LOOP_UNTIL_TIMEOUT(ui32Timeoutus)
	{
		ui32ActualValue = OSReadHWReg32((void *)pui32LinMemAddr, 0) & ui32Mask;

        if ((ui32MaxValue>ui32Value) ?  (ui32ActualValue >= ui32Value && ui32ActualValue<= ui32MaxValue) : (ui32ActualValue >= ui32Value || ui32ActualValue <=ui32MaxValue))
		{
			return PVRSRV_OK;
		}

		if (gpsPVRSRVData->eServicesState != PVRSRV_SERVICES_STATE_OK)
		{
			return PVRSRV_ERROR_TIMEOUT;
		}

		if (bAllowPreemption)
		{
			OSSleepms(ui32PollPeriodus / 1000);
		}
		else
		{
			OSWaitus(ui32PollPeriodus);
		}
	} END_LOOP_UNTIL_TIMEOUT();

	PVR_DPF((PVR_DBG_ERROR,"PollForValueKM: Timeout. Expected 0x%x but found 0x%x (mask 0x%x).",
			ui32Value, ui32ActualValue, ui32Mask));

	return PVRSRV_ERROR_TIMEOUT;
#endif /* NO_HARDWARE */
}

/*
	PVRSRVPollForGEValueKM
*/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVPollForGEValueKM (volatile IMG_UINT32	*pui32LinMemAddr,
												IMG_UINT32			ui32Value)
{
	return PollForGEValueKM(pui32LinMemAddr, ui32Value,
						  MAX_HW_TIME_US,
						  MAX_HW_TIME_US/WAIT_TRY_COUNT,
						  IMG_FALSE);
}
#endif /* MTK_POWER_OFF_TIMING */

/*
	PollForValueKM
*/
static
PVRSRV_ERROR IMG_CALLCONV PollForValueKM (volatile IMG_UINT32 __iomem *	pui32LinMemAddr,
										  IMG_UINT32			ui32Value,
										  IMG_UINT32			ui32Mask,
										  IMG_UINT32			ui32Timeoutus,
										  IMG_UINT32			ui32PollPeriodus,
										  IMG_BOOL				bAllowPreemption)
{
#if defined(NO_HARDWARE)
	PVR_UNREFERENCED_PARAMETER(pui32LinMemAddr);
	PVR_UNREFERENCED_PARAMETER(ui32Value);
	PVR_UNREFERENCED_PARAMETER(ui32Mask);
	PVR_UNREFERENCED_PARAMETER(ui32Timeoutus);
	PVR_UNREFERENCED_PARAMETER(ui32PollPeriodus);
	PVR_UNREFERENCED_PARAMETER(bAllowPreemption);
	return PVRSRV_OK;
#else
	IMG_UINT32	ui32ActualValue = 0xFFFFFFFFU; /* Initialiser only required to prevent incorrect warning */

	if (bAllowPreemption)
	{
		PVR_ASSERT(ui32PollPeriodus >= 1000);
	}

	LOOP_UNTIL_TIMEOUT(ui32Timeoutus)
	{
		ui32ActualValue = OSReadHWReg32(pui32LinMemAddr, 0) & ui32Mask;

		if (ui32ActualValue == ui32Value)
		{
			return PVRSRV_OK;
		}

		if (gpsPVRSRVData->eServicesState != PVRSRV_SERVICES_STATE_OK)
		{
			return PVRSRV_ERROR_TIMEOUT;
		}

		if (bAllowPreemption)
		{
			OSSleepms(ui32PollPeriodus / 1000);
		}
		else
		{
			OSWaitus(ui32PollPeriodus);
		}
	} END_LOOP_UNTIL_TIMEOUT();

	PVR_DPF((PVR_DBG_ERROR,"PollForValueKM: Timeout. Expected 0x%x but found 0x%x (mask 0x%x).",
			ui32Value, ui32ActualValue, ui32Mask));

	return PVRSRV_ERROR_TIMEOUT;
#endif /* NO_HARDWARE */
}


/*
	PVRSRVPollForValueKM
*/
PVRSRV_ERROR IMG_CALLCONV PVRSRVPollForValueKM (volatile IMG_UINT32	__iomem *pui32LinMemAddr,
												IMG_UINT32			ui32Value,
												IMG_UINT32			ui32Mask)
{
	return PollForValueKM(pui32LinMemAddr, ui32Value, ui32Mask,
						  MAX_HW_TIME_US,
						  MAX_HW_TIME_US/WAIT_TRY_COUNT,
						  IMG_FALSE);
}

static
PVRSRV_ERROR IMG_CALLCONV WaitForValueKM(volatile IMG_UINT32 __iomem *pui32LinMemAddr,
                                         IMG_UINT32           ui32Value,
                                         IMG_UINT32           ui32Mask,
                                         IMG_BOOL             bHoldBridgeLock)
{
#if defined(NO_HARDWARE)
	PVR_UNREFERENCED_PARAMETER(pui32LinMemAddr);
	PVR_UNREFERENCED_PARAMETER(ui32Value);
	PVR_UNREFERENCED_PARAMETER(ui32Mask);
	return PVRSRV_OK;
#else

	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	IMG_HANDLE hOSEvent;
	PVRSRV_ERROR eError;
	PVRSRV_ERROR eErrorWait;
	IMG_UINT32 ui32ActualValue;

	eError = OSEventObjectOpen(psPVRSRVData->hGlobalEventObject, &hOSEvent);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVWaitForValueKM: Failed to setup EventObject with error (%d)", eError));
		goto EventObjectOpenError;
	}

	eError = PVRSRV_ERROR_TIMEOUT;

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		ui32ActualValue = (OSReadDeviceMem32(pui32LinMemAddr) & ui32Mask);

		if (ui32ActualValue == ui32Value)
		{
			/* Expected value has been found */
			eError = PVRSRV_OK;
			break;
		}
		else if (psPVRSRVData->eServicesState != PVRSRV_SERVICES_STATE_OK)
		{
			/* Services in bad state, don't wait any more */
			eError = PVRSRV_ERROR_NOT_READY;
			break;
		}
		else
		{
			/* wait for event and retry */
			eErrorWait = bHoldBridgeLock ? OSEventObjectWaitAndHoldBridgeLock(hOSEvent) : OSEventObjectWait(hOSEvent);
			if (eErrorWait != PVRSRV_OK  &&  eErrorWait != PVRSRV_ERROR_TIMEOUT)
			{
				PVR_DPF((PVR_DBG_WARNING,"PVRSRVWaitForValueKM: Waiting for value failed with error %d. Expected 0x%x but found 0x%x (Mask 0x%08x). Retrying",
							eErrorWait,
							ui32Value,
							ui32ActualValue,
							ui32Mask));
			}
		}
	} END_LOOP_UNTIL_TIMEOUT();

	OSEventObjectClose(hOSEvent);

	/* One last check in case the object wait ended after the loop timeout... */
	if (eError != PVRSRV_OK  &&  (OSReadDeviceMem32(pui32LinMemAddr) & ui32Mask) == ui32Value)
	{
		eError = PVRSRV_OK;
	}

	/* Provide event timeout information to aid the Device Watchdog Thread... */
	if (eError == PVRSRV_OK)
	{
		psPVRSRVData->ui32GEOConsecutiveTimeouts = 0;
	}
	else if (eError == PVRSRV_ERROR_TIMEOUT)
	{
		psPVRSRVData->ui32GEOConsecutiveTimeouts++;
	}

EventObjectOpenError:

	return eError;

#endif /* NO_HARDWARE */
}

/*
	PVRSRVWaitForValueKM
*/
PVRSRV_ERROR IMG_CALLCONV PVRSRVWaitForValueKM (volatile IMG_UINT32	__iomem *pui32LinMemAddr,
												IMG_UINT32			ui32Value,
												IMG_UINT32			ui32Mask)
{
	/* In this case we are NOT retaining bridge lock while waiting
	   for bridge lock. */
	return WaitForValueKM(pui32LinMemAddr, ui32Value, ui32Mask, IMG_FALSE);
}

/*
	PVRSRVWaitForValueKMAndHoldBridgeLock
*/
PVRSRV_ERROR IMG_CALLCONV PVRSRVWaitForValueKMAndHoldBridgeLockKM(volatile IMG_UINT32 __iomem *pui32LinMemAddr,
                                                                  IMG_UINT32          ui32Value,
                                                                  IMG_UINT32          ui32Mask)
{
	return WaitForValueKM(pui32LinMemAddr, ui32Value, ui32Mask, IMG_TRUE);
}

int PVRSRVGetDriverStatus(void)
{
	return PVRSRVGetPVRSRVData()->eServicesState;
}

/*
	PVRSRVSystemHasCacheSnooping
*/
IMG_BOOL PVRSRVSystemHasCacheSnooping(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	if ((psDevConfig->eCacheSnoopingMode != PVRSRV_DEVICE_SNOOP_NONE) &&
		(psDevConfig->eCacheSnoopingMode != PVRSRV_DEVICE_SNOOP_EMULATED))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_BOOL PVRSRVSystemSnoopingIsEmulated(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	if (psDevConfig->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_EMULATED)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_BOOL PVRSRVSystemSnoopingOfCPUCache(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	if ((psDevConfig->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_CPU_ONLY) ||
		(psDevConfig->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_CROSS))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_BOOL PVRSRVSystemSnoopingOfDeviceCache(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	if ((psDevConfig->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_DEVICE_ONLY) ||
		(psDevConfig->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_CROSS))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_BOOL PVRSRVSystemHasNonMappableLocalMemory(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	return psDevConfig->bHasNonMappableLocalMemory;
}

/*
	PVRSRVSystemWaitCycles
*/
void PVRSRVSystemWaitCycles(PVRSRV_DEVICE_CONFIG *psDevConfig, IMG_UINT32 ui32Cycles)
{
	/* Delay in us */
	IMG_UINT32 ui32Delayus = 1;

	/* obtain the device freq */
	if (psDevConfig->pfnClockFreqGet != NULL)
	{
		IMG_UINT32 ui32DeviceFreq;

		ui32DeviceFreq = psDevConfig->pfnClockFreqGet(psDevConfig->hSysData);

		ui32Delayus = (ui32Cycles*1000000)/ui32DeviceFreq;

		if (ui32Delayus == 0)
		{
			ui32Delayus = 1;
		}
	}

	OSWaitus(ui32Delayus);
}

static void *
PVRSRVSystemInstallDeviceLISR_Match_AnyVaCb(PVRSRV_DEVICE_NODE *psDeviceNode,
											va_list va)
{
	void *pvOSDevice = va_arg(va, void *);

	if (psDeviceNode->psDevConfig->pvOSDevice == pvOSDevice)
	{
		return psDeviceNode;
	}

	return NULL;
}

PVRSRV_ERROR PVRSRVSystemInstallDeviceLISR(void *pvOSDevice,
										   IMG_UINT32 ui32IRQ,
										   const IMG_CHAR *pszName,
										   PFN_LISR pfnLISR,
										   void *pvData,
										   IMG_HANDLE *phLISRData)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE *psDeviceNode;

	psDeviceNode =
		List_PVRSRV_DEVICE_NODE_Any_va(psPVRSRVData->psDeviceNodeList,
									   &PVRSRVSystemInstallDeviceLISR_Match_AnyVaCb,
									   pvOSDevice);
	if (!psDeviceNode)
	{
		/* Device can't be found in the list so it isn't in the system */
		PVR_DPF((PVR_DBG_ERROR, "%s: device %p with irq %d is not present",
				 __func__, pvOSDevice, ui32IRQ));
		return PVRSRV_ERROR_INVALID_DEVICE;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "%s: device %p with irq %d / %s ",
                                 __func__, pvOSDevice, ui32IRQ, pszName));

	return SysInstallDeviceLISR(psDeviceNode->psDevConfig->hSysData, ui32IRQ,
								pszName, pfnLISR, pvData, phLISRData);
}

PVRSRV_ERROR PVRSRVSystemUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
	return SysUninstallDeviceLISR(hLISRData);
}

PVRSRV_ERROR
PVRSRVSystemBIFTilingHeapGetXStride(PVRSRV_DEVICE_CONFIG *psDevConfig,
									IMG_UINT32 uiHeapNum,
									IMG_UINT32 *puiXStride)
{
	PVR_ASSERT(puiXStride != NULL);

	if (uiHeapNum < 1 || uiHeapNum > psDevConfig->ui32BIFTilingHeapCount)
	{
		*puiXStride = 0;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*puiXStride = psDevConfig->pui32BIFTilingHeapConfigs[uiHeapNum - 1];

	return PVRSRV_OK;
}

PVRSRV_ERROR
PVRSRVSystemBIFTilingGetConfig(PVRSRV_DEVICE_CONFIG  *psDevConfig,
                               RGXFWIF_BIFTILINGMODE *peBifTilingMode,
                               IMG_UINT32            *puiNumHeaps)
{
	*peBifTilingMode = psDevConfig->eBIFTilingMode;
	*puiNumHeaps = psDevConfig->ui32BIFTilingHeapCount;
	return PVRSRV_OK;
}

#if defined(SUPPORT_GPUVIRT_VALIDATION) && defined(EMULATOR)
void SetAxiProtOSid(IMG_UINT32 ui32OSid, IMG_BOOL bState)
{
	SysSetAxiProtOSid(ui32OSid, bState);
	return;
}

void SetTrustedDeviceAceEnabled(void)
{
	SysSetTrustedDeviceAceEnabled();

	return;
}
#endif

#if defined(SUPPORT_RGX)
PVRSRV_ERROR IMG_CALLCONV PVRSRVCreateHWPerfHostThread(IMG_UINT32 ui32Timeout)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!ui32Timeout)
		return PVRSRV_ERROR_INVALID_PARAMS;

	OSLockAcquire(gpsPVRSRVData->hHWPerfHostPeriodicThread_Lock);

	/* Create only once */
	if (gpsPVRSRVData->hHWPerfHostPeriodicThread == NULL)
	{
		/* Create the HWPerf event object */
		eError = OSEventObjectCreate("PVRSRV_HWPERFHOSTPERIODIC_EVENTOBJECT", &gpsPVRSRVData->hHWPerfHostPeriodicEvObj);

		if (eError == PVRSRV_OK)
		{
			gpsPVRSRVData->bHWPerfHostThreadStop = IMG_FALSE;
			gpsPVRSRVData->ui32HWPerfHostThreadTimeout = ui32Timeout;
			/* Create a thread which is used to periodically emit host stream packets */
			eError = OSThreadCreate(&gpsPVRSRVData->hHWPerfHostPeriodicThread,
				"pvr_hwperf_host",
				HWPerfPeriodicHostEventsThread,
				NULL, IMG_TRUE, gpsPVRSRVData);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create HWPerf host periodic thread", __func__));
			}
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: OSEventObjectCreate failed", __func__));
		}
	}
	/* If the thread has already been created then just update the timeout and wake up thread */
	else
	{
		gpsPVRSRVData->ui32HWPerfHostThreadTimeout = ui32Timeout;
		eError = OSEventObjectSignal(gpsPVRSRVData->hHWPerfHostPeriodicEvObj);
		PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");
	}

	OSLockRelease(gpsPVRSRVData->hHWPerfHostPeriodicThread_Lock);
	return eError;
}

PVRSRV_ERROR IMG_CALLCONV PVRSRVDestroyHWPerfHostThread(void)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	OSLockAcquire(gpsPVRSRVData->hHWPerfHostPeriodicThread_Lock);

	/* Stop and cleanup the HWPerf periodic thread */
	if (gpsPVRSRVData->hHWPerfHostPeriodicThread)
	{
		if (gpsPVRSRVData->hHWPerfHostPeriodicEvObj)
		{
			gpsPVRSRVData->bHWPerfHostThreadStop = IMG_TRUE;
			eError = OSEventObjectSignal(gpsPVRSRVData->hHWPerfHostPeriodicEvObj);
			PVR_LOG_IF_ERROR(eError, "OSEventObjectSignal");
		}
		LOOP_UNTIL_TIMEOUT(OS_THREAD_DESTROY_TIMEOUT_US)
		{
			eError = OSThreadDestroy(gpsPVRSRVData->hHWPerfHostPeriodicThread);
			if (PVRSRV_OK == eError)
			{
				gpsPVRSRVData->hHWPerfHostPeriodicThread = NULL;
				break;
			}
			OSWaitus(OS_THREAD_DESTROY_TIMEOUT_US/OS_THREAD_DESTROY_RETRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();
		PVR_LOG_IF_ERROR(eError, "OSThreadDestroy");

		if (gpsPVRSRVData->hHWPerfHostPeriodicEvObj)
		{
			eError = OSEventObjectDestroy(gpsPVRSRVData->hHWPerfHostPeriodicEvObj);
			gpsPVRSRVData->hHWPerfHostPeriodicEvObj = NULL;
			PVR_LOG_IF_ERROR(eError, "OSEventObjectDestroy");
		}
	}

	OSLockRelease(gpsPVRSRVData->hHWPerfHostPeriodicThread_Lock);
	return eError;
}
#endif

static PVRSRV_ERROR _VzDeviceCreate(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	RA_BASE_T uBase;
	RA_LENGTH_T uSize;
	IMG_UINT ui32OSID;
	IMG_UINT64 ui64Size;
	PVRSRV_ERROR eError;
	PHYS_HEAP *psPhysHeap;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	PHYS_HEAP_TYPE eHeapType;
	IMG_UINT32 ui32NumOfHeapRegions;
	PVRSRV_VZ_RET_IF_MODE(DRIVER_MODE_NATIVE, PVRSRV_OK);

	/* First, register device GPU physical heap based on physheap config */
	psPhysHeap = psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL];
	ui32NumOfHeapRegions = PhysHeapNumberOfRegions(psPhysHeap);
	eHeapType = PhysHeapGetType(psPhysHeap);

	/* Normally, for GPU UMA physheap, use OS services but here we override this
	   if said physheap is DMA/UMA carve-out; for this create an RA to manage it */
	if (eHeapType == PHYS_HEAP_TYPE_UMA || eHeapType == PHYS_HEAP_TYPE_DMA)
	{
		if (ui32NumOfHeapRegions)
		{
			eError = PhysHeapRegionGetCpuPAddr(psPhysHeap, 0, &sCpuPAddr);
			if (eError != PVRSRV_OK)
			{
				PVR_ASSERT(IMG_FALSE);
				goto e0;
			}

			eError = PhysHeapRegionGetSize(psPhysHeap, 0, &ui64Size);
			if (eError != PVRSRV_OK)
			{
				PVR_ASSERT(IMG_FALSE);
				goto e0;
			}

			eError = PhysHeapRegionGetDevPAddr(psPhysHeap, 0, &sDevPAddr);
			if (eError != PVRSRV_OK)
			{
				PVR_ASSERT(IMG_FALSE);
				goto e0;
			}
		}
		else
		{
			sDevPAddr.uiAddr = (IMG_UINT64)0;
			sCpuPAddr.uiAddr = (IMG_UINT64)0;
			ui64Size = (IMG_UINT64)0;
		}

		if (sCpuPAddr.uiAddr && sDevPAddr.uiAddr && ui64Size)
		{
			psDeviceNode->ui32NumOfLocalMemArenas = ui32NumOfHeapRegions;
			PVR_ASSERT(ui32NumOfHeapRegions == 1);

			PVR_DPF((PVR_DBG_MESSAGE, "===== UMA (carve-out) memory, 1st phys heap (gpu)"));

			PVR_DPF((PVR_DBG_MESSAGE, "Creating RA for gpu memory 0x%016"IMG_UINT64_FMTSPECX"-0x%016"IMG_UINT64_FMTSPECX,
					(IMG_UINT64) sCpuPAddr.uiAddr, sCpuPAddr.uiAddr + ui64Size - 1));

			uBase = sDevPAddr.uiAddr;
			uSize = (RA_LENGTH_T) ui64Size;
			PVR_ASSERT(uSize == ui64Size);

			psDeviceNode->apsLocalDevMemArenas = OSAllocMem(sizeof(RA_ARENA*));
			PVR_ASSERT(psDeviceNode->apsLocalDevMemArenas);
			psDeviceNode->apszRANames = OSAllocMem(sizeof(IMG_PCHAR));
			PVR_ASSERT(psDeviceNode->apszRANames);
			psDeviceNode->apszRANames[0] = OSAllocMem(PVRSRV_MAX_RA_NAME_LENGTH);
			PVR_ASSERT(psDeviceNode->apszRANames[0]);

			OSSNPrintf(psDeviceNode->apszRANames[0], PVRSRV_MAX_RA_NAME_LENGTH,
						"%s gpu mem", psDeviceNode->psDevConfig->pszName);

			psDeviceNode->apsLocalDevMemArenas[0] =
				RA_Create(psDeviceNode->apszRANames[0],
							OSGetPageShift(),	/* Use OS page size, keeps things simple */
							RA_LOCKCLASS_0,		/* This arena doesn't use any other arenas. */
							NULL,				/* No Import */
							NULL,				/* No free import */
							NULL,				/* No import handle */
							IMG_FALSE);
			if (psDeviceNode->apsLocalDevMemArenas[0] == NULL)
			{
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e0;
			}

			if (!RA_Add(psDeviceNode->apsLocalDevMemArenas[0], uBase, uSize, 0 , NULL))
			{
				RA_Delete(psDeviceNode->apsLocalDevMemArenas[0]);
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e0;
			}

			/* Replace the UMA allocator with LMA allocator */
			psDeviceNode->pfnDevPxAlloc = LMA_PhyContigPagesAlloc;
			psDeviceNode->pfnDevPxFree = LMA_PhyContigPagesFree;
			psDeviceNode->pfnDevPxMap = LMA_PhyContigPagesMap;
			psDeviceNode->pfnDevPxUnMap = LMA_PhyContigPagesUnmap;
			psDeviceNode->pfnDevPxClean = LMA_PhyContigPagesClean;
			psDeviceNode->uiMMUPxLog2AllocGran = OSGetPageShift();
			psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL] = PhysmemNewLocalRamBackedPMR;
		}
	}
	else
	{
		/* LMA heap sanity check */
		PVR_ASSERT(ui32NumOfHeapRegions);
	}

	/* Next, register device firmware physical heap based on heap config */
	psPhysHeap = psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL];
	ui32NumOfHeapRegions = PhysHeapNumberOfRegions(psPhysHeap);
	eHeapType = PhysHeapGetType(psPhysHeap);
	PVR_ASSERT(eHeapType != PHYS_HEAP_TYPE_UNKNOWN);

	PVR_DPF((PVR_DBG_MESSAGE, "===== LMA/DMA/UMA (carve-out) memory, 2nd phys heap (fw)"));

	if (ui32NumOfHeapRegions)
	{
		eError = PhysHeapRegionGetCpuPAddr(psPhysHeap, 0, &sCpuPAddr);
		if (eError != PVRSRV_OK)
		{
			PVR_ASSERT(IMG_FALSE);
			goto e0;
		}

		eError = PhysHeapRegionGetSize(psPhysHeap, 0, &ui64Size);
		if (eError != PVRSRV_OK)
		{
			PVR_ASSERT(IMG_FALSE);
			goto e0;
		}

		eError = PhysHeapRegionGetDevPAddr(psPhysHeap, 0, &sDevPAddr);
		if (eError != PVRSRV_OK)
		{
			PVR_ASSERT(IMG_FALSE);
			goto e0;
		}
	}
	else
	{
		sDevPAddr.uiAddr = (IMG_UINT64)0;
		sCpuPAddr.uiAddr = (IMG_UINT64)0;
		ui64Size = (IMG_UINT64)0;
	}

	if (ui32NumOfHeapRegions)
	{
#if defined(SUPPORT_RGX)
		PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;
		RA_LENGTH_T uConfigSize = RGX_FIRMWARE_CONFIG_HEAP_SIZE;
		RA_LENGTH_T uMainSize = 0;

		uMainSize = (RA_LENGTH_T) RGXGetFwMainHeapSize(psDeviceNode->pvDevice);

		SysVzGetPhysHeapOrigin(psDeviceNode->psDevConfig,
							   PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL,
							   &eHeapOrigin);

		PVR_DPF((PVR_DBG_MESSAGE, "Creating RA for  fw memory 0x%016"IMG_UINT64_FMTSPECX"-0x%016"IMG_UINT64_FMTSPECX,
				(IMG_UINT64) sCpuPAddr.uiAddr, sCpuPAddr.uiAddr + ui64Size - 1));

		/* Now we construct RA to manage FW heap */
		uBase = sDevPAddr.uiAddr;
		uSize = (RA_LENGTH_T) ui64Size;
		PVR_ASSERT(sCpuPAddr.uiAddr && uSize == ui64Size);
		if (eHeapType != PHYS_HEAP_TYPE_LMA)
		{
			/* On some LMA config, fw base starts at zero */
			PVR_ASSERT(sDevPAddr.uiAddr);
		}

		/* All vz drivers go through this motion, loop terminates early for guest driver(s) */
		for (ui32OSID = 0; ui32OSID < RGXFW_NUM_OS; ui32OSID++)
		{
			RA_BASE_T uOSIDConfigBase,	uOSIDMainBase;
#if defined(SUPPORT_RGX)
			if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_HOST) && ui32OSID == 0)
			{
				uOSIDMainBase = uBase;
				uOSIDConfigBase = uOSIDMainBase + RGXGetFwMainHeapSize((PVRSRV_RGXDEV_INFO *) psDeviceNode->pvDevice);
			}
			else
#else
			/* Assert here if SUPPORT_RGX = 0 as function is broken */
			PVR_DPF((PVR_DBG_ERROR, "%s: Func not operational with SUPPORT_RGX = 0", __func__));
			PVR_ASSERT(0);
#endif
			{
				uOSIDConfigBase = uBase + (ui32OSID * RGX_FIRMWARE_RAW_HEAP_SIZE);
				uOSIDMainBase = uOSIDConfigBase + uConfigSize;
			}

			OSSNPrintf(psDeviceNode->szKernelFwConfigRAName[ui32OSID], sizeof(psDeviceNode->szKernelFwConfigRAName[ui32OSID]),
									"%s fw mem", psDeviceNode->psDevConfig->pszName);

			psDeviceNode->psKernelFwConfigMemArena[ui32OSID] =
				RA_Create(psDeviceNode->szKernelFwConfigRAName[ui32OSID],
							OSGetPageShift(),		/* Use OS page size, keeps things simple */
							RA_LOCKCLASS_0,			/* This arena doesn't use any other arenas. */
							NULL,				/* No Import */
							NULL,				/* No free import */
							NULL,				/* No import handle */
							IMG_FALSE);
			if (psDeviceNode->psKernelFwConfigMemArena[ui32OSID] == NULL)
			{
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e1;
			}

			if (!RA_Add(psDeviceNode->psKernelFwConfigMemArena[ui32OSID], uOSIDConfigBase, uConfigSize, 0 , NULL))
			{
				RA_Delete(psDeviceNode->psKernelFwConfigMemArena[ui32OSID]);
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e1;
			}

			OSSNPrintf(psDeviceNode->szKernelFwMainRAName[ui32OSID], sizeof(psDeviceNode->szKernelFwMainRAName[ui32OSID]),
						"%s fw mem", psDeviceNode->psDevConfig->pszName);

			psDeviceNode->psKernelFwMainMemArena[ui32OSID] =
				RA_Create(psDeviceNode->szKernelFwMainRAName[ui32OSID],
							OSGetPageShift(),		/* Use OS page size, keeps things simple */
							RA_LOCKCLASS_0,			/* This arena doesn't use any other arenas. */
							NULL,				/* No Import */
							NULL,				/* No free import */
							NULL,				/* No import handle */
							IMG_FALSE);
			if (psDeviceNode->psKernelFwMainMemArena[ui32OSID] == NULL)
			{
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e1;
			}

			if (!RA_Add(psDeviceNode->psKernelFwMainMemArena[ui32OSID], uOSIDMainBase, uMainSize, 0 , NULL))
			{
				RA_Delete(psDeviceNode->psKernelFwMainMemArena[ui32OSID]);
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto e1;
			}

			/* Guest drivers should not initialize subsequent array entries as the driver depends on this */
			if (eHeapOrigin != PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST || PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
			{
				break;
			}
		}
#else
		PVR_UNREFERENCED_PARAMETER(ui32OSID);
		PVR_DPF((PVR_DBG_ERROR,"Support RGX undef"));
		PVR_ASSERT(0);
#endif
		/* Fw physheap is always managed by LMA PMR factory */
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL] = PhysmemNewLocalRamBackedPMR;
	}

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_HOST))
	{
		/* Guest Fw physheap is a pseudo-heap which is always managed by LMA PMR factory and exclusively used
		   by the host driver. For this pseudo-heap, we do not create an actual heap meta-data to represent
		   it seeing it's only used during guest driver FW initialisation so this saves us having to provide
		   heap pfnCpuPAddrToDevPAddr/pfnDevPAddrToCpuPAddr callbacks here which are not needed as the host
		   driver will _never_ access this guest firmware heap - instead we reuse the real FW heap meta-data */
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_FW_GUEST] = PhysmemNewLocalRamBackedPMR;
		psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_GUEST] =
											psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL];
	}

	return PVRSRV_OK;
#if defined(SUPPORT_RGX)
e1:
	_VzDeviceDestroy(psDeviceNode);
#endif
e0:
	return eError;
}

static void _VzDeviceDestroy(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	IMG_UINT ui32OSID;
	IMG_UINT64 ui64Size;
	PHYS_HEAP *psPhysHeap;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	PHYS_HEAP_TYPE eHeapType;
	IMG_UINT32 ui32NumOfHeapRegions;
	PVRSRV_VZ_RETN_IF_MODE(DRIVER_MODE_NATIVE);

	/* First, unregister device firmware physical heap based on heap config */
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_HOST))
	{
		/* Remove pseudo-heap pointer, rest of heap deinitialization is unaffected */
		psDeviceNode->pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_FW_GUEST] = NULL;
		psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_GUEST] = NULL;
	}

	psPhysHeap = psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL];
	ui32NumOfHeapRegions = PhysHeapNumberOfRegions(psPhysHeap);

	if (ui32NumOfHeapRegions)
	{
		for (ui32OSID = 0; ui32OSID < RGXFW_NUM_OS; ui32OSID++)
		{
			if (psDeviceNode->psKernelFwMainMemArena[ui32OSID])
			{
				RA_Delete(psDeviceNode->psKernelFwMainMemArena[ui32OSID]);
				psDeviceNode->psKernelFwMainMemArena[ui32OSID] = NULL;
			}

			if (psDeviceNode->psKernelFwConfigMemArena[ui32OSID])
			{
				RA_Delete(psDeviceNode->psKernelFwConfigMemArena[ui32OSID]);
				psDeviceNode->psKernelFwConfigMemArena[ui32OSID] = NULL;
			}
		}
	}

	/* Next, unregister device GPU physical heap based on heap config */
	psPhysHeap = psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL];
	ui32NumOfHeapRegions = PhysHeapNumberOfRegions(psPhysHeap);
	eHeapType = PhysHeapGetType(psPhysHeap);

	if (eHeapType == PHYS_HEAP_TYPE_UMA || eHeapType == PHYS_HEAP_TYPE_DMA)
	{
		if (ui32NumOfHeapRegions)
		{
			if (PhysHeapRegionGetCpuPAddr(psPhysHeap, 0, &sCpuPAddr) != PVRSRV_OK)
			{
				PVR_ASSERT(IMG_FALSE);
				return;
			}

			if (PhysHeapRegionGetSize(psPhysHeap, 0, &ui64Size) != PVRSRV_OK)
			{
				PVR_ASSERT(IMG_FALSE);
				return;
			}

			if (PhysHeapRegionGetDevPAddr(psPhysHeap, 0, &sDevPAddr) != PVRSRV_OK)
			{
				PVR_ASSERT(IMG_FALSE);
				return;
			}
		}
		else
		{
			sDevPAddr.uiAddr = (IMG_UINT64)0;
			sCpuPAddr.uiAddr = (IMG_UINT64)0;
			ui64Size = (IMG_UINT64)0;
		}

		if (sCpuPAddr.uiAddr && sDevPAddr.uiAddr && ui64Size)
		{
			if (psDeviceNode->apsLocalDevMemArenas && psDeviceNode->apsLocalDevMemArenas[0])
			{
				RA_Delete(psDeviceNode->apsLocalDevMemArenas[0]);
				psDeviceNode->apsLocalDevMemArenas[0] = NULL;
				OSFreeMem(psDeviceNode->apsLocalDevMemArenas);
				psDeviceNode->apsLocalDevMemArenas = NULL;
			}

			if (psDeviceNode->apszRANames)
			{
				OSFreeMem(psDeviceNode->apszRANames[0]);
				psDeviceNode->apszRANames[0] = NULL;
				OSFreeMem(psDeviceNode->apszRANames);
				psDeviceNode->apszRANames = NULL;
			}
		}
	}
}

PVRSRV_ERROR IMG_CALLCONV PVRSRVVzRegisterFirmwarePhysHeap(PVRSRV_DEVICE_NODE *psDeviceNode,
															IMG_DEV_PHYADDR sDevPAddr,
															IMG_UINT64 ui64DevPSize,
															IMG_UINT32 uiOSID)
{
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;
	PHYS_HEAP *psPhysHeap;
	PVRSRV_ERROR eError;

	/*
	   This is called by the host driver only, it creates an RA to manage this guest firmware
	   physheaps so we fail the call if an invalid guest OSID is supplied.
	*/
	PVRSRV_VZ_RET_IF_NOT_MODE(DRIVER_MODE_HOST, PVRSRV_ERROR_INTERNAL_ERROR);
	PVR_DPF((PVR_DBG_MESSAGE, "===== Registering OSID: %d fw physheap memory", uiOSID));
	PVR_LOGR_IF_FALSE(((uiOSID > 0)&&(uiOSID < RGXFW_NUM_OS)), "Invalid guest OSID", PVRSRV_ERROR_INVALID_PARAMS);

	/* Verify guest size with host size (support only same sized FW heaps) */
	psPhysHeap = psDeviceNode->apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL];

	if (ui64DevPSize != RGX_FIRMWARE_RAW_HEAP_SIZE)
	{
		PVR_DPF((PVR_DBG_WARNING,
				"OSID: %d fw physheap size 0x%"IMG_UINT64_FMTSPECX" differs from host fw phyheap size 0x%X",
				uiOSID,
				ui64DevPSize,
				RGX_FIRMWARE_RAW_HEAP_SIZE));

		PVR_DPF((PVR_DBG_WARNING,
				"Truncating OSID: %d requested fw physheap to: 0x%X\n",
				uiOSID,
				RGX_FIRMWARE_RAW_HEAP_SIZE));
	}

	PVR_DPF((PVR_DBG_MESSAGE, "Creating RA for fw 0x%016"IMG_UINT64_FMTSPECX"-0x%016"IMG_UINT64_FMTSPECX" [DEV/PA]",
			(IMG_UINT64) sDevPAddr.uiAddr, sDevPAddr.uiAddr + RGX_FIRMWARE_RAW_HEAP_SIZE - 1));

	SysVzGetPhysHeapOrigin(psDeviceNode->psDevConfig,
						   PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL,
						   &eHeapOrigin);
	PVR_LOGR_IF_FALSE((eHeapOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST),
					  "PVRSRVVzRegisterFirmwarePhysHeap: Host PVZ config: Invalid PVZ setup\n"
					  "=>: all driver types (i.e. host/guest) must use same FW heap origin",
					  PVRSRV_ERROR_INVALID_PARAMS);

	OSSNPrintf(psDeviceNode->szKernelFwRawRAName[uiOSID],
			   sizeof(psDeviceNode->szKernelFwRawRAName[uiOSID]),
			   "[OSID: %d]: raw guest fw mem", uiOSID);

	eError = _VzConstructRAforFwHeap(&psDeviceNode->psKernelFwRawMemArena[uiOSID],
									 psDeviceNode->szKernelFwRawRAName[uiOSID],
									 sDevPAddr.uiAddr,
									 RGX_FIRMWARE_RAW_HEAP_SIZE);
	if (eError == PVRSRV_OK)
	{
		psDeviceNode->ui64RABase[uiOSID] = sDevPAddr.uiAddr;
	}

	return eError;
}

PVRSRV_ERROR IMG_CALLCONV PVRSRVVzUnregisterFirmwarePhysHeap(PVRSRV_DEVICE_NODE *psDeviceNode,
															 IMG_UINT32 uiOSID)
{
	PVRSRV_DEVICE_PHYS_HEAP_ORIGIN eHeapOrigin;

	PVRSRV_VZ_RET_IF_NOT_MODE(DRIVER_MODE_HOST, PVRSRV_ERROR_INTERNAL_ERROR);
	PVR_DPF((PVR_DBG_MESSAGE, "===== Unregistering OSID: %d fw physheap memory", uiOSID));
	PVR_LOGR_IF_FALSE(((uiOSID > 0)&&(uiOSID < RGXFW_NUM_OS)), "Invalid guest OSID", PVRSRV_ERROR_INVALID_PARAMS);

	SysVzGetPhysHeapOrigin(psDeviceNode->psDevConfig,
						   PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL,
						   &eHeapOrigin);
	PVR_LOGR_IF_FALSE((eHeapOrigin == PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST),
					  "PVRSRVVzUnregisterFirmwarePhysHeap: Host PVZ config: Invalid PVZ setup\n"
					  "=>: all driver types (i.e. host/guest) must use same FW heap origin",
					  PVRSRV_ERROR_INVALID_PARAMS);

	_VzTearDownRAforFwHeap(&psDeviceNode->psKernelFwRawMemArena[uiOSID], (IMG_UINT64)psDeviceNode->ui64RABase[uiOSID]);

	return PVRSRV_OK;
}

static PVRSRV_ERROR _VzConstructRAforFwHeap(RA_ARENA **ppsArena, IMG_CHAR *szName,
											IMG_UINT64 uBase, RA_LENGTH_T uSize)
{
	PVRSRV_ERROR eError;

	/* Construct RA to manage FW Raw heap */
	*ppsArena = RA_Create(szName,
						OSGetPageShift(),		/* Use host page size, keeps things simple */
						RA_LOCKCLASS_0,			/* This arena doesn't use any other arenas */
						NULL,					/* No Import */
						NULL,					/* No free import */
						NULL,					/* No import handle */
						IMG_FALSE);
	eError = (*ppsArena == NULL) ? (PVRSRV_ERROR_OUT_OF_MEMORY) : (PVRSRV_OK);

	if (eError == PVRSRV_OK && !RA_Add(*ppsArena, uBase, uSize, 0 , NULL))
	{
		RA_Delete(*ppsArena);
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	return eError;
}

static void _VzTearDownRAforFwHeap(RA_ARENA **ppsArena, IMG_UINT64 uBase)
{
	RA_Free(*ppsArena, uBase);
	RA_Delete(*ppsArena);
	*ppsArena = NULL;
}

/*****************************************************************************
 End of file (pvrsrv.c)
*****************************************************************************/
