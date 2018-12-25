/*************************************************************************/ /*!
@File           sync_native_server.c
@Title          Native implementation of server fence sync interface.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    The server implementation of software native synchronisation.
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

#include "osfunc.h"
#include "rgxhwperf.h"
#include "pvrsrv_sync_server.h"

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#include <linux/sw_sync.h>
#else
#include <../drivers/staging/android/sw_sync.h>
#endif

#include "kernel_compatibility.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0))
static inline int sync_fence_get_status(struct sync_fence *psFence)
{
	return psFence->status;
}

static inline struct sync_timeline *sync_pt_parent(struct sync_pt *pt)
{
	return pt->parent;
}

static inline int sync_pt_get_status(struct sync_pt *pt)
{
	return pt->status;
}

static inline ktime_t sync_pt_get_timestamp(struct sync_pt *pt)
{
	return pt->timestamp;
}

#define for_each_sync_pt(s, f, c)							\
	list_for_each_entry((s), &(f)->pt_list_head, pt_list)
#else
static inline int sync_fence_get_status(struct sync_fence *psFence)
{
	int iStatus = atomic_read(&psFence->status);

	/*
	 * When Android sync was rebased on top of fences the sync_fence status
	 * values changed from 0 meaning 'active' to 'signalled' and, likewise,
	 * values greater than 0 went from meaning 'signalled' to 'active'
	 * (where the value corresponds to the number of active sync points).
	 *
	 * Convert to the old style status values.
	 */
	return iStatus > 0 ? 0 : iStatus ? iStatus : 1;
}

static inline int sync_pt_get_status(struct sync_pt *pt)
{
	/* No error state for raw dma-buf fences */
	return fence_is_signaled(&pt->base) ? 1 : 0;
}

static inline ktime_t sync_pt_get_timestamp(struct sync_pt *pt)
{
	return pt->base.timestamp;
}

#define for_each_sync_pt(s, f, c)							   \
	for ((c) = 0, (s) = (struct sync_pt *)(f)->cbs[0].sync_pt; \
	     (c) < (f)->num_fences;								   \
	     (c)++,   (s) = (struct sync_pt *)(f)->cbs[c].sync_pt)
#endif

#if 0
static void _DumpFence(const char *psczName, struct sync_fence *psFence,
					DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
					void *pvDumpDebugFile)
{
	struct sync_pt *psPt;
	char szTime[16]  = { '\0' };
	char szVal1[64]  = { '\0' };
	char szVal2[64]  = { '\0' };
	char szVal3[132] = { '\0' };
	int iStatus = sync_fence_get_status(psFence);
	int i;

	PVR_UNREFERENCED_PARAMETER(i);

	PVR_DUMPDEBUG_LOG("\t  %s: [%p] %s: %s", psczName, psFence, psFence->name,
					   (iStatus > 0 ? "signalled" : iStatus ? "error" : "active"));

	for_each_sync_pt(psPt, psFence, i)
	{
		struct sync_timeline *psTimeline = sync_pt_parent(psPt);
		ktime_t timestamp = sync_pt_get_timestamp(psPt);
		struct timeval tv = ktime_to_timeval(timestamp);
		int iPtStatus = sync_pt_get_status(psPt);

		snprintf(szTime, sizeof(szTime), "@%ld.%06ld", tv.tv_sec, tv.tv_usec);

		if (psTimeline->ops->pt_value_str &&
			psTimeline->ops->timeline_value_str)
		{
			psTimeline->ops->pt_value_str(psPt, szVal1, sizeof(szVal1));
			psTimeline->ops->timeline_value_str(psTimeline, szVal2, sizeof(szVal2));
			snprintf(szVal3, sizeof(szVal3), ": %s / %s", szVal1, szVal2);
		}

		PVR_DUMPDEBUG_LOG("\t    %s %s%s%s", psTimeline->name,
						   (iPtStatus > 0 ? "signalled" : iPtStatus ? "error" : "active"),
						   (iPtStatus > 0 ? szTime : ""),
						   szVal3);
	}
}
#endif

PVRSRV_ERROR SyncSWTimelineFenceCreateKM(PVRSRV_TIMELINE iSWTimeline,
                                         IMG_UINT32 ui32NextSyncPtValue,
                                         const IMG_CHAR *pszFenceName,
                                         PVRSRV_FENCE *piOutputFence)
{
	PVRSRV_ERROR eError;
	struct file *psFile;
	struct sw_sync_timeline *psSWTimeline;
	struct sync_fence *psFence = NULL;
	struct sync_pt *psPt;
	int iFd = get_unused_fd();

	if (iFd < 0)
	{
		eError = PVRSRV_ERROR_RESOURCE_UNAVAILABLE;
		goto ErrorOut;
	}

	psFile = fget(iSWTimeline);
	if (!psFile || !psFile->private_data)
	{
		/* unrecognised timeline */
		eError = PVRSRV_ERROR_RESOURCE_UNAVAILABLE;
		goto ErrorOut;
	}

	psSWTimeline = (struct sw_sync_timeline *)psFile->private_data;

	psPt = sw_sync_pt_create(psSWTimeline, ui32NextSyncPtValue);
	fput(psFile);
	if(!psPt)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorPutFd;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)) && \
    defined(CHROMIUMOS_WORKAROUNDS_KERNEL318)
	psFence = sync_fence_create(pszFenceName, &psPt->base);
#else
	psFence = sync_fence_create(pszFenceName, psPt);
#endif
	if(!psFence)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorFreePoint;
	}

	sync_fence_install(psFence, iFd);

	*piOutputFence = (PVRSRV_FENCE)iFd;
	return PVRSRV_OK;

ErrorFreePoint:
	sync_pt_free(psPt);
ErrorPutFd:
	put_unused_fd(iFd);
ErrorOut:
	return eError;
}

PVRSRV_ERROR SyncSWTimelineAdvanceKM(SYNC_TIMELINE_OBJ pvSWTimelineObj)
{
	struct file *psFile;
	struct sw_sync_timeline *psSWTimeline;

	if (pvSWTimelineObj == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psFile = (struct file *)pvSWTimelineObj;
	psSWTimeline = (struct sw_sync_timeline *)psFile->private_data;
	sw_sync_timeline_inc(psSWTimeline, 1);
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWTimelineReleaseKM(SYNC_TIMELINE_OBJ pvSWTimelineObj)
{
	struct file *psFile;

	if (pvSWTimelineObj == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psFile = (struct file *)pvSWTimelineObj;
	fput(psFile);
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWTimelineFenceReleaseKM(SYNC_FENCE_OBJ pvSWFenceObj)
{
	sync_fence_put(pvSWFenceObj);
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWTimelineFenceWaitKM(SYNC_FENCE_OBJ pvSWFenceObj,
                                       IMG_UINT32 uiTimeout)
{
	int err;

	err = sync_fence_wait(pvSWFenceObj, uiTimeout);
	/* -ETIME means active. In this case we will retry later again. If the
	 * return value is an error or zero we will close this fence and
	 * proceed. This makes sure that we are not getting stuck here when a
	 * fence changes into an error state for whatever reason. */
	if (err == -ETIME)
	{
#if 0
		_DumpFence("sync_fence_wait", pvSWFenceObj, NULL, NULL);
#endif
		return PVRSRV_ERROR_TIMEOUT;
	}
	else if (err != 0)
	{
		return PVRSRV_ERROR_FAILED_DEPENDENCIES;
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWGetTimelineObj(PVRSRV_TIMELINE iSWTimeline,
                                  SYNC_TIMELINE_OBJ *ppvSWTimelineObj)
{
	struct file *psFile = fget(iSWTimeline);

	if(psFile == NULL || psFile->private_data == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	*ppvSWTimelineObj = (SYNC_TIMELINE_OBJ*)psFile;
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWGetFenceObj(PVRSRV_FENCE iSWFence,
                               SYNC_FENCE_OBJ *ppvSWFenceObj)
{
	struct file *psFile = fget(iSWFence);

	if(psFile == NULL || psFile->private_data == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	*ppvSWFenceObj = (SYNC_FENCE_OBJ*)psFile->private_data;
	return PVRSRV_OK;
}

IMG_BOOL PVRSRVIsTimelineValidKM(PVRSRV_TIMELINE iTimeline)
{
	return (iTimeline > -1) ? IMG_TRUE : IMG_FALSE;
}

IMG_BOOL PVRSRVIsFenceValidKM(PVRSRV_FENCE iFence)
{
	return (iFence > -1) ? IMG_TRUE : IMG_FALSE;
}
