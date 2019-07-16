/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File           services_kernel_client.h
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

/* This file contains a partial redefinition of the PowerVR Services 5
 * interface for use by components which are checkpatch clean. This
 * header is included by the unrefined, non-checkpatch clean headers
 * to ensure that prototype/typedef/macro changes break the build.
 */

#ifndef __SERVICES_KERNEL_CLIENT__
#define __SERVICES_KERNEL_CLIENT__

#include "pvrsrv_error.h"

#include <linux/types.h>

#include "pvrsrv_sync_km.h"
#include "sync_checkpoint_external.h"

#ifndef __pvrsrv_defined_struct_enum__

/* sync_external.h */

struct PVRSRV_CLIENT_SYNC_PRIM {
	volatile __u32 *pui32LinAddr;
};

struct PVRSRV_CLIENT_SYNC_PRIM_OP {
	__u32 ui32Flags;
	struct pvrsrv_sync_prim *psSync;
	__u32 ui32FenceValue;
	__u32 ui32UpdateValue;
};

#else /* __pvrsrv_defined_struct_enum__ */

struct PVRSRV_CLIENT_SYNC_PRIM;
struct PVRSRV_CLIENT_SYNC_PRIM_OP;

enum tag_img_bool;

#endif /* __pvrsrv_defined_struct_enum__ */

struct _PMR_;
struct _PVRSRV_DEVICE_NODE_;
struct dma_buf;
struct SYNC_PRIM_CONTEXT;

/* pvr_notifier.h */

#ifndef _CMDCOMPNOTIFY_PFN_
typedef void (*PFN_CMDCOMP_NOTIFY)(void *hCmdCompHandle);
#define _CMDCOMPNOTIFY_PFN_
#endif
enum PVRSRV_ERROR PVRSRVRegisterCmdCompleteNotify(void **phNotify,
	PFN_CMDCOMP_NOTIFY pfnCmdCompleteNotify, void *hPrivData);
enum PVRSRV_ERROR PVRSRVUnregisterCmdCompleteNotify(void *hNotify);
void PVRSRVCheckStatus(void *hCmdCompCallerHandle);

#define DEBUG_REQUEST_DC               0
#define DEBUG_REQUEST_SERVERSYNC       1
#define DEBUG_REQUEST_SYS              2
#define DEBUG_REQUEST_ANDROIDSYNC      3
#define DEBUG_REQUEST_LINUXFENCE       4
#define DEBUG_REQUEST_SYNCCHECKPOINT   5
#define DEBUG_REQUEST_HTB              6
#define DEBUG_REQUEST_APPHINT          7
#define DEBUG_REQUEST_FALLBACKSYNC     8

#define DEBUG_REQUEST_VERBOSITY_LOW    0
#define DEBUG_REQUEST_VERBOSITY_MEDIUM 1
#define DEBUG_REQUEST_VERBOSITY_HIGH   2
#define DEBUG_REQUEST_VERBOSITY_MAX    DEBUG_REQUEST_VERBOSITY_HIGH

#define DD_VERB_LVL_ENABLED(_verbLvl, _verbLvlChk) ((_verbLvl) >= (_verbLvlChk))

#ifndef _DBGNOTIFY_PFNS_
typedef void (DUMPDEBUG_PRINTF_FUNC)(void *pvDumpDebugFile,
	const char *fmt, ...) __printf(2, 3);
typedef void (*PFN_DBGREQ_NOTIFY) (void *hDebugRequestHandle,
	__u32 ui32VerbLevel,
	DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
	void *pvDumpDebugFile);
#define _DBGNOTIFY_PFNS_
#endif
enum PVRSRV_ERROR PVRSRVRegisterDbgRequestNotify(void **phNotify,
	struct _PVRSRV_DEVICE_NODE_ *psDevNode,
	PFN_DBGREQ_NOTIFY pfnDbgRequestNotify,
	__u32 ui32RequesterID,
	void *hDbgRequestHandle);
enum PVRSRV_ERROR PVRSRVUnregisterDbgRequestNotify(void *hNotify);

/* physmem_dmabuf.h */

struct dma_buf *PhysmemGetDmaBuf(struct _PMR_ *psPMR);

/* pvrsrv.h */

enum PVRSRV_ERROR PVRSRVAcquireGlobalEventObjectKM(void **phGlobalEventObject);
enum PVRSRV_ERROR PVRSRVReleaseGlobalEventObjectKM(void *hGlobalEventObject);

/* sync.h */

enum PVRSRV_ERROR SyncPrimContextCreate(
	struct _PVRSRV_DEVICE_NODE_ *psDevConnection,
	struct SYNC_PRIM_CONTEXT **phSyncPrimContext);
void SyncPrimContextDestroy(struct SYNC_PRIM_CONTEXT *hSyncPrimContext);

enum PVRSRV_ERROR SyncPrimAlloc(struct SYNC_PRIM_CONTEXT *hSyncPrimContext,
	struct PVRSRV_CLIENT_SYNC_PRIM **ppsSync, const char *pszClassName);
enum PVRSRV_ERROR SyncPrimFree(struct PVRSRV_CLIENT_SYNC_PRIM *psSync);
enum PVRSRV_ERROR SyncPrimGetFirmwareAddr(
	struct PVRSRV_CLIENT_SYNC_PRIM *psSync,
	__u32 *sync_addr);
enum PVRSRV_ERROR SyncPrimSet(struct PVRSRV_CLIENT_SYNC_PRIM *psSync,
	__u32 ui32Value);

/* osfunc.h */
#if defined(PVRSRV_USE_BRIDGE_LOCK)
void OSAcquireBridgeLock(void);
void OSReleaseBridgeLock(void);
#endif
enum PVRSRV_ERROR OSEventObjectWait(void *hOSEventKM);
enum PVRSRV_ERROR OSEventObjectOpen(void *hEventObject, void **phOSEventKM);
enum PVRSRV_ERROR OSEventObjectClose(void *hOSEventKM);
__u32 OSGetCurrentClientProcessIDKM(void);

/* srvkm.h */

enum PVRSRV_ERROR PVRSRVDeviceCreate(void *pvOSDevice,
	int i32UMIdentifier,
	struct _PVRSRV_DEVICE_NODE_ **ppsDeviceNode);
enum PVRSRV_ERROR PVRSRVDeviceDestroy(
	struct _PVRSRV_DEVICE_NODE_ *psDeviceNode);
const char *PVRSRVGetErrorString(enum PVRSRV_ERROR eError);

#ifndef _CHECKPOINT_PFNS_
typedef PVRSRV_ERROR (*PFN_SYNC_CHECKPOINT_FENCE_RESOLVE_FN)(PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext, PVRSRV_FENCE fence, u32 *nr_checkpoints, PSYNC_CHECKPOINT **checkpoint_handles, u64 *fence_uid);

#ifndef _CHECKPOINT_PFNS_
typedef PVRSRV_ERROR (*PFN_SYNC_CHECKPOINT_FENCE_CREATE_FN)(
		const char *fence_name,
		PVRSRV_TIMELINE timeline,
		PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext,
		PVRSRV_FENCE *new_fence,
		u64 *fence_uid,
		void **fence_finalise_data,
		PSYNC_CHECKPOINT *new_checkpoint_handle,
		void **timeline_update_sync,
		__u32 *timeline_update_value);
#endif

#ifndef _CHECKPOINT_PFNS_
typedef PVRSRV_ERROR (*PFN_SYNC_CHECKPOINT_FENCE_ROLLBACK_DATA_FN)(PVRSRV_FENCE fence_to_rollback, void *finalise_data);
#endif

#ifndef _CHECKPOINT_PFNS_
typedef PVRSRV_ERROR (*PFN_SYNC_CHECKPOINT_FENCE_FINALISE_FN)(PVRSRV_FENCE fence_to_finalise, void *finalise_data);
#endif

#ifndef _CHECKPOINT_PFNS_
typedef __u32 (*PFN_SYNC_CHECKPOINT_DUMP_INFO_ON_STALLED_UFOS_FN)(__u32 num_ufos, __u32 *vaddrs);
#endif

#ifndef _CHECKPOINT_PFNS_
typedef enum tag_img_bool (*PFN_SYNC_CHECKPOINT_UFO_HAS_SIGNALLED_FN)(
	__u32 ui32FwAddr, __u32 ui32Value);
typedef enum PVRSRV_ERROR (*PFN_SYNC_CHECKPOINT_SIGNAL_WAITERS_FN)(void);
typedef void(*PFN_SYNC_CHECKPOINT_CHECK_STATE_FN)(void);
#endif

/* This is the function that kick code will call in a NO_HARDWARE build only after
 * sync checkpoints have been manually signalled, to allow the OS native sync
 * implementation to update its timelines (as the usual callback notification
 * of signalled checkpoints is not supported for NO_HARDWARE).
 */
#ifndef _CHECKPOINT_PFNS_
typedef void (*PFN_SYNC_CHECKPOINT_NOHW_UPDATE_TIMELINES_FN)(void *private_data);
typedef void (*PFN_SYNC_CHECKPOINT_FREE_CHECKPOINT_LIST_MEM_FN)(void *mem_ptr);

#define SYNC_CHECKPOINT_IMPL_MAX_STRLEN 20

typedef struct {
	PFN_SYNC_CHECKPOINT_FENCE_RESOLVE_FN pfnFenceResolve;
	PFN_SYNC_CHECKPOINT_FENCE_CREATE_FN pfnFenceCreate;
	PFN_SYNC_CHECKPOINT_FENCE_ROLLBACK_DATA_FN pfnFenceDataRollback;
	PFN_SYNC_CHECKPOINT_FENCE_FINALISE_FN pfnFenceFinalise;
	PFN_SYNC_CHECKPOINT_NOHW_UPDATE_TIMELINES_FN pfnNoHWUpdateTimelines;
	PFN_SYNC_CHECKPOINT_FREE_CHECKPOINT_LIST_MEM_FN pfnFreeCheckpointListMem;
	PFN_SYNC_CHECKPOINT_DUMP_INFO_ON_STALLED_UFOS_FN pfnDumpInfoOnStalledUFOs;
	PFN_SYNC_CHECKPOINT_UFO_HAS_SIGNALLED_FN pfnCheckpointHasSignalled;
	PFN_SYNC_CHECKPOINT_CHECK_STATE_FN pfnCheckState;
	PFN_SYNC_CHECKPOINT_SIGNAL_WAITERS_FN pfnSignalWaiters;
	char pszImplName[SYNC_CHECKPOINT_IMPL_MAX_STRLEN];
} PFN_SYNC_CHECKPOINT_STRUCT;

enum PVRSRV_ERROR SyncCheckpointRegisterFunctions(PFN_SYNC_CHECKPOINT_STRUCT *psSyncCheckpointPfns);

#define _CHECKPOINT_PFNS_
#endif

/* sync_checkpoint.h */
enum PVRSRV_ERROR SyncCheckpointContextCreate(struct _PVRSRV_DEVICE_NODE_ *psDevConnection, PSYNC_CHECKPOINT_CONTEXT *phSyncCheckpointContext);
enum PVRSRV_ERROR SyncCheckpointContextDestroy(PSYNC_CHECKPOINT_CONTEXT hSyncCheckpointContext);
void SyncCheckpointContextRef(PSYNC_CHECKPOINT_CONTEXT psContext);
void SyncCheckpointContextUnref(PSYNC_CHECKPOINT_CONTEXT psContext);
enum PVRSRV_ERROR SyncCheckpointAlloc(PSYNC_CHECKPOINT_CONTEXT psSyncContext, PVRSRV_TIMELINE timeline, PVRSRV_FENCE fence, const char *pszCheckpointName, PSYNC_CHECKPOINT *ppsSyncCheckpoint);
void SyncCheckpointSignal(PSYNC_CHECKPOINT psSyncCheckpoint, u32 fence_sync_flags);
void SyncCheckpointError(PSYNC_CHECKPOINT psSyncCheckpoint, u32 fence_sync_flags);
enum tag_img_bool SyncCheckpointIsSignalled(PSYNC_CHECKPOINT psSyncCheckpoint, u32 fence_sync_flags);
enum tag_img_bool SyncCheckpointIsErrored(PSYNC_CHECKPOINT psSyncCheckpoint, u32 fence_sync_flags);
enum PVRSRV_ERROR SyncCheckpointTakeRef(PSYNC_CHECKPOINT psSyncCheckpoint);
enum PVRSRV_ERROR SyncCheckpointDropRef(PSYNC_CHECKPOINT psSyncCheckpoint);
void SyncCheckpointFree(PSYNC_CHECKPOINT psSyncCheckpoint);
__u32 SyncCheckpointGetFirmwareAddr(PSYNC_CHECKPOINT psSyncCheckpoint);
void SyncCheckpointCCBEnqueued(PSYNC_CHECKPOINT psSyncCheckpoint);
__u32 SyncCheckpointGetId(PSYNC_CHECKPOINT psSyncCheckpoint);
__u32 SyncCheckpointGetEnqueuedCount(PSYNC_CHECKPOINT psSyncCheckpoint);
__u32 SyncCheckpointGetReferenceCount(PSYNC_CHECKPOINT psSyncCheckpoint);
PVRSRV_TIMELINE SyncCheckpointGetTimeline(PSYNC_CHECKPOINT psSyncCheckpoint);
const char *SyncCheckpointGetStateString(PSYNC_CHECKPOINT psSyncCheckpoint);

#endif

#endif /* __SERVICES_KERNEL_CLIENT__ */
