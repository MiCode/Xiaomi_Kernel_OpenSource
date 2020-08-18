/*************************************************************************/ /*!
@File
@Title          RGX HW Performance header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX HWPerf functions
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

#ifndef RGXHWPERF_H_
#define RGXHWPERF_H_

#include "img_types.h"
#include "img_defs.h"
#include "pvrsrv_error.h"

#include "device.h"
#include "connection_server.h"
#include "rgxdevice.h"
#include "rgx_hwperf.h"

/* HWPerf host buffer size constraints in KBs */
#define HWPERF_HOST_TL_STREAM_SIZE_DEFAULT PVRSRV_APPHINT_HWPERFHOSTBUFSIZEINKB
#define HWPERF_HOST_TL_STREAM_SIZE_MIN     (32U)
#define HWPERF_HOST_TL_STREAM_SIZE_MAX     (1024U)

/******************************************************************************
 * RGX HW Performance Data Transport Routines
 *****************************************************************************/

PVRSRV_ERROR RGXHWPerfDataStoreCB(PVRSRV_DEVICE_NODE* psDevInfo);

PVRSRV_ERROR RGXHWPerfInit(PVRSRV_RGXDEV_INFO *psRgxDevInfo);
PVRSRV_ERROR RGXHWPerfInitOnDemandResources(PVRSRV_RGXDEV_INFO* psRgxDevInfo);
void RGXHWPerfDeinit(PVRSRV_RGXDEV_INFO *psRgxDevInfo);
void RGXHWPerfInitAppHintCallbacks(const PVRSRV_DEVICE_NODE *psDeviceNode);
void RGXHWPerfClientInitAppHintCallbacks(void);

/******************************************************************************
 * RGX HW Performance Profiling API(s)
 *****************************************************************************/

PVRSRV_ERROR PVRSRVRGXCtrlHWPerfKM(
	CONNECTION_DATA      * psConnection,
	PVRSRV_DEVICE_NODE   * psDeviceNode,
	 RGX_HWPERF_STREAM_ID  eStreamId,
	IMG_BOOL               bToggle,
	IMG_UINT64             ui64Mask);


PVRSRV_ERROR PVRSRVRGXConfigEnableHWPerfCountersKM(
	CONNECTION_DATA    * psConnection,
	PVRSRV_DEVICE_NODE * psDeviceNode,
	IMG_UINT32         ui32ArrayLen,
	RGX_HWPERF_CONFIG_CNTBLK * psBlockConfigs);

PVRSRV_ERROR PVRSRVRGXCtrlHWPerfCountersKM(
	CONNECTION_DATA    * psConnection,
	PVRSRV_DEVICE_NODE * psDeviceNode,
	IMG_BOOL           bEnable,
	IMG_UINT32         ui32ArrayLen,
	IMG_UINT16         * psBlockIDs);

PVRSRV_ERROR PVRSRVRGXConfigCustomCountersKM(
	CONNECTION_DATA    * psConnection,
	PVRSRV_DEVICE_NODE * psDeviceNode,
	IMG_UINT16           ui16CustomBlockID,
	IMG_UINT16           ui16NumCustomCounters,
	IMG_UINT32         * pui32CustomCounterIDs);

/******************************************************************************
 * RGX HW Performance Host Stream API
 *****************************************************************************/

PVRSRV_ERROR RGXHWPerfHostInit(PVRSRV_RGXDEV_INFO *psRgxDevInfo, IMG_UINT32 ui32BufSizeKB);
PVRSRV_ERROR RGXHWPerfHostInitOnDemandResources(PVRSRV_RGXDEV_INFO* psRgxDevInfo);
void RGXHWPerfHostDeInit(PVRSRV_RGXDEV_INFO	*psRgxDevInfo);

void RGXHWPerfHostSetEventFilter(PVRSRV_RGXDEV_INFO *psRgxDevInfo,
                                 IMG_UINT32 ui32Filter);

void RGXHWPerfHostPostEnqEvent(PVRSRV_RGXDEV_INFO *psRgxDevInfo,
                               RGX_HWPERF_KICK_TYPE eEnqType,
                               IMG_UINT32 ui32Pid,
                               IMG_UINT32 ui32FWDMContext,
                               IMG_UINT32 ui32ExtJobRef,
                               IMG_UINT32 ui32IntJobRef,
                               IMG_UINT64 ui64CheckFenceUID,
                               IMG_UINT64 ui64UpdateFenceUID,
                               IMG_UINT64 ui64DeadlineInus,
                               IMG_UINT64 ui64CycleEstimate);

void RGXHWPerfHostPostAllocEvent(PVRSRV_RGXDEV_INFO *psRgxDevInfo,
                                 RGX_HWPERF_HOST_RESOURCE_TYPE eAllocType,
                                 IMG_UINT64 ui64UID,
                                 IMG_UINT32 ui32PID,
                                 IMG_UINT32 ui32FWAddr,
                                 const IMG_CHAR *psName,
                                 IMG_UINT32 ui32NameSize);

void RGXHWPerfHostPostFreeEvent(PVRSRV_RGXDEV_INFO *psRgxDevInfo,
                                RGX_HWPERF_HOST_RESOURCE_TYPE eFreeType,
                                IMG_UINT64 ui64UID,
                                IMG_UINT32 ui32PID,
                                IMG_UINT32 ui32FWAddr);

void RGXHWPerfHostPostModifyEvent(PVRSRV_RGXDEV_INFO *psRgxDevInfo,
                                  RGX_HWPERF_HOST_RESOURCE_TYPE eModifyType,
                                  IMG_UINT64 ui64NewUID,
                                  IMG_UINT64 ui64UID1,
                                  IMG_UINT64 ui64UID2,
                                  const IMG_CHAR *psName,
                                  IMG_UINT32 ui32NameSize);

void RGXHWPerfHostPostUfoEvent(PVRSRV_RGXDEV_INFO *psRgxDevInfo,
                               RGX_HWPERF_UFO_EV eUfoType,
                               RGX_HWPERF_UFO_DATA_ELEMENT *psUFOData);

void RGXHWPerfHostPostClkSyncEvent(PVRSRV_RGXDEV_INFO *psRgxDevInfo);

IMG_BOOL RGXHWPerfHostIsEventEnabled(PVRSRV_RGXDEV_INFO *psRgxDevInfo, RGX_HWPERF_HOST_EVENT_TYPE eEvent);

#define _RGX_HWPERF_HOST_FILTER(CTX, EV) \
		(((PVRSRV_RGXDEV_INFO *)CTX->psDeviceNode->pvDevice)->ui32HWPerfHostFilter \
		& RGX_HWPERF_EVENT_MASK_VALUE(EV))

#define _RGX_DEVICE_INFO_FROM_CTX(CTX) \
		((PVRSRV_RGXDEV_INFO *)CTX->psDeviceNode->pvDevice)

#define _RGX_DEVICE_INFO_FROM_NODE(DEVNODE) \
		((PVRSRV_RGXDEV_INFO *)DEVNODE->pvDevice)

/* Deadline and cycle estimate is not supported for all ENQ events */
#define NO_DEADLINE 0
#define NO_CYCEST   0


#if defined(SUPPORT_RGX)

/**
 * This macro checks if HWPerfHost and the event are enabled and if they are
 * it posts event to the HWPerfHost stream.
 *
 * @param C      Kick context
 * @param P      Pid of kicking process
 * @param X      Related FW context
 * @param E      External job reference
 * @param I      Job ID
 * @param K      Kick type
 * @param CHKUID Check fence UID
 * @param UPDUID Update fence UID
 * @param D      Deadline
 * @param CE     Cycle estimate
 */
#define RGX_HWPERF_HOST_ENQ(C, P, X, E, I, K, CHKUID, UPDUID, D, CE) \
		do { \
			if (_RGX_HWPERF_HOST_FILTER(C, RGX_HWPERF_HOST_ENQ)) \
			{ \
				RGXHWPerfHostPostEnqEvent(_RGX_DEVICE_INFO_FROM_CTX(C), \
				                          (K), (P), (X), (E), (I), \
				                          (CHKUID), (UPDUID), (D), (CE)); \
			} \
		} while (0)

/**
 * This macro checks if HWPerfHost and the event are enabled and if they are
 * it posts event to the HWPerfHost stream.
 *
 * @param I Device Info pointer
 * @param T Host UFO event type
 * @param D Pointer to UFO data
 */
#define RGX_HWPERF_HOST_UFO(I, T, D) \
		do { \
			if (RGXHWPerfHostIsEventEnabled((I), RGX_HWPERF_HOST_UFO)) \
			{ \
				RGXHWPerfHostPostUfoEvent((I), (T), (D)); \
			} \
		} while (0)

/**
 * This macro checks if HWPerfHost and the event are enabled and if they are
 * it posts event to the HWPerfHost stream.
 *
 * @param D Device node pointer
 * @param T Host ALLOC event type
 * @param FWADDR sync firmware address
 * @param N string containing sync name
 * @param Z string size including null terminating character
 */
#define RGX_HWPERF_HOST_ALLOC(D, T, FWADDR, N, Z) \
		do { \
			if (RGXHWPerfHostIsEventEnabled(_RGX_DEVICE_INFO_FROM_NODE(D), RGX_HWPERF_HOST_ALLOC)) \
			{ \
				RGXHWPerfHostPostAllocEvent(_RGX_DEVICE_INFO_FROM_NODE(D), \
				                            RGX_HWPERF_HOST_RESOURCE_TYPE_##T, 0, 0, \
				                            (FWADDR), (N), (Z)); \
			} \
		} while (0)

/**
 * This macro checks if HWPerfHost and the event are enabled and if they are
 * it posts event to the HWPerfHost stream.
 *
 * @param D Device Node pointer
 * @param T Host ALLOC event type
 * @param UID ID of input object
 * @param PID ID of allocating process
 * @param FWADDR sync firmware address
 * @param N string containing sync name
 * @param Z string size including null terminating character
 */
#define RGX_HWPERF_HOST_ALLOC_FENCE_SYNC(D, T, UID, PID, FWADDR, N, Z)  \
		do { \
			if (RGXHWPerfHostIsEventEnabled(_RGX_DEVICE_INFO_FROM_NODE(D), RGX_HWPERF_HOST_ALLOC)) \
			{ \
				RGXHWPerfHostPostAllocEvent(_RGX_DEVICE_INFO_FROM_NODE(D), \
				                            RGX_HWPERF_HOST_RESOURCE_TYPE_##T, \
				                            (UID), (PID), (FWADDR), (N), (Z)); \
			} \
		} while (0)

/**
 * This macro checks if HWPerfHost and the event are enabled and if they are
 * it posts event to the HWPerfHost stream.
 *
 * @param D Device Node pointer
 * @param T Host ALLOC event type
 * @param FWADDR sync firmware address
 */
#define RGX_HWPERF_HOST_FREE(D, T, FWADDR) \
		do { \
			if (RGXHWPerfHostIsEventEnabled(_RGX_DEVICE_INFO_FROM_NODE(D), RGX_HWPERF_HOST_FREE)) \
			{ \
				RGXHWPerfHostPostFreeEvent(_RGX_DEVICE_INFO_FROM_NODE(D), \
				                           RGX_HWPERF_HOST_RESOURCE_TYPE_##T, \
				                           (0), (0), (FWADDR)); \
			} \
		} while (0)

/**
 * This macro checks if HWPerfHost and the event are enabled and if they are
 * it posts event to the HWPerfHost stream.
 *
 * @param D Device Node pointer
 * @param T Host ALLOC event type
 * @param UID ID of input object
 * @param PID ID of allocating process
 * @param FWADDR sync firmware address
 */
#define RGX_HWPERF_HOST_FREE_FENCE_SYNC(D, T, UID, PID, FWADDR) \
		do { \
			if (RGXHWPerfHostIsEventEnabled(_RGX_DEVICE_INFO_FROM_NODE(D), RGX_HWPERF_HOST_FREE)) \
			{ \
				RGXHWPerfHostPostFreeEvent(_RGX_DEVICE_INFO_FROM_NODE(D), \
				                           RGX_HWPERF_HOST_RESOURCE_TYPE_##T, \
				                           (UID), (PID), (FWADDR)); \
			} \
		} while (0)

/**
 * This macro checks if HWPerfHost and the event are enabled and if they are
 * it posts event to the HWPerfHost stream.
 *
 * @param D Device Node pointer
 * @param T Host ALLOC event type
 * @param NEWUID ID of output object
 * @param UID1 ID of first input object
 * @param UID2 ID of second input object
 * @param N string containing new object's name
 * @param Z string size including null terminating character
 */
#define RGX_HWPERF_HOST_MODIFY_FENCE_SYNC(D, T, NEWUID, UID1, UID2, N, Z) \
		do { \
			if (RGXHWPerfHostIsEventEnabled(_RGX_DEVICE_INFO_FROM_NODE(D), RGX_HWPERF_HOST_MODIFY)) \
			{ \
				RGXHWPerfHostPostModifyEvent(_RGX_DEVICE_INFO_FROM_NODE(D), \
				                             RGX_HWPERF_HOST_RESOURCE_TYPE_##T, \
				                             (NEWUID), (UID1), (UID2), N, Z); \
			} \
		} while (0)


/**
 * This macro checks if HWPerfHost and the event are enabled and if they are
 * it posts event to the HWPerfHost stream.
 *
 * @param I Device info pointer
 */
#define RGX_HWPERF_HOST_CLK_SYNC(I) \
		do { \
			if (RGXHWPerfHostIsEventEnabled((I), RGX_HWPERF_HOST_CLK_SYNC)) \
			{ \
				RGXHWPerfHostPostClkSyncEvent((I)); \
			} \
		} while (0)


#else

#define RGX_HWPERF_HOST_ENQ(C, P, X, E, I, K, CHKUID, UPDUID, D, CE)
#define RGX_HWPERF_HOST_UFO(I, T, D)
#define RGX_HWPERF_HOST_ALLOC(D, T, FWADDR, N, Z)
#define RGX_HWPERF_HOST_ALLOC_FENCE_SYNC(D, T, UID, PID, FWADDR, N, Z)
#define RGX_HWPERF_HOST_FREE(D, T, FWADDR)
#define RGX_HWPERF_HOST_FREE_FENCE_SYNC(D, T, UID, PID, FWADDR)
#define RGX_HWPERF_HOST_MODIFY_FENCE_SYNC(D, T, NEWUID, UID1, UID2, N, Z)
#define RGX_HWPERF_HOST_CLK_SYNC(I)

#endif


/******************************************************************************
 * RGX HW Performance To FTrace Profiling API(s)
 *****************************************************************************/

#if defined(SUPPORT_GPUTRACE_EVENTS)

PVRSRV_ERROR RGXHWPerfFTraceGPUInitSupport(void);
void RGXHWPerfFTraceGPUDeInitSupport(void);

PVRSRV_ERROR RGXHWPerfFTraceGPUInitDevice(PVRSRV_DEVICE_NODE *psDeviceNode);
void RGXHWPerfFTraceGPUDeInitDevice(PVRSRV_DEVICE_NODE *psDeviceNode);

void RGXHWPerfFTraceGPUEnqueueEvent(PVRSRV_RGXDEV_INFO *psDevInfo,
		IMG_UINT32 ui32ExternalJobRef, IMG_UINT32 ui32InternalJobRef,
		RGX_HWPERF_KICK_TYPE eKickType);

PVRSRV_ERROR RGXHWPerfFTraceGPUEventsEnabledSet(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_BOOL bNewValue);

void RGXHWPerfFTraceGPUThread(void *pvData);

#endif

/******************************************************************************
 * RGX HW utils functions
 *****************************************************************************/

const IMG_CHAR *RGXHWPerfKickTypeToStr(RGX_HWPERF_KICK_TYPE eKickType);

#endif /* RGXHWPERF_H_ */
