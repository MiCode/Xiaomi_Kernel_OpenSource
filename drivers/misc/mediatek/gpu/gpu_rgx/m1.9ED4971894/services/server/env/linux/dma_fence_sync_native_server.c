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
#include <linux/sync_file.h>
#include <linux/version.h>

#include "kernel_compatibility.h"

#include "pvr_sync.h"
#include "pvr_counting_timeline.h"

PVRSRV_ERROR SyncSWTimelineFenceCreateKM(PVRSRV_TIMELINE iSWTimeline,
                                         IMG_UINT32 ui32NextSyncPtValue,
                                         const IMG_CHAR *pszFenceName,
                                         PVRSRV_FENCE *piOutputFence)
{
	PVRSRV_ERROR eError;
	struct pvr_counting_fence_timeline *psSWTimeline;
	struct dma_fence *psFence = NULL;
	struct sync_file *psSyncFile = NULL;
	int iFd = get_unused_fd_flags(O_CLOEXEC);

	if (iFd < 0)
	{
		eError = PVRSRV_ERROR_RESOURCE_UNAVAILABLE;
		goto ErrorOut;
	}

	psSWTimeline = pvr_sync_get_sw_timeline(iSWTimeline);
	if (!psSWTimeline)
	{
		/* unrecognised timeline */
		eError = PVRSRV_ERROR_RESOURCE_UNAVAILABLE;
		goto ErrorPutFd;
	}

	psFence = pvr_counting_fence_create(psSWTimeline, ui32NextSyncPtValue);
	pvr_counting_fence_timeline_put(psSWTimeline);
	if(!psFence)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorPutFd;
	}

	psSyncFile = sync_file_create(psFence);
	if (!psSyncFile)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorPutFence;
	}

	fd_install(iFd, psSyncFile->file);

	*piOutputFence = (PVRSRV_FENCE)iFd;
	return PVRSRV_OK;

ErrorPutFence:
	dma_fence_put(psFence);
ErrorPutFd:
	put_unused_fd(iFd);
ErrorOut:
	return eError;
}

PVRSRV_ERROR SyncSWTimelineAdvanceKM(SYNC_TIMELINE_OBJ pvSWTimelineObj)
{
	if (pvSWTimelineObj == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	pvr_counting_fence_timeline_inc(pvSWTimelineObj, 1);
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWTimelineReleaseKM(SYNC_TIMELINE_OBJ pvSWTimelineObj)
{
	if (pvSWTimelineObj == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	pvr_counting_fence_timeline_put(pvSWTimelineObj);
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWTimelineFenceReleaseKM(SYNC_FENCE_OBJ pvSWFenceObj)
{
	dma_fence_put(pvSWFenceObj);
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWTimelineFenceWaitKM(SYNC_FENCE_OBJ pvSWFenceObj,
                                       IMG_UINT32 uiTimeout)
{
	long lJiffies = msecs_to_jiffies(uiTimeout);
	int err;

	err = dma_fence_wait_timeout(pvSWFenceObj, true, lJiffies);
	/* dma_fence_wait_timeout returns:
	 * 0 on timeout,
	 * -ERESTARTSYS if interrupted
	 *  or the 'remaining timeout' on success*/
	if (err == 0)
	{
#if 0
		_DumpFence("sync_fence_wait", pvSWFenceObj, NULL, NULL);
#endif
		return PVRSRV_ERROR_TIMEOUT;
	}
	else if (err < 0)
	{
		return PVRSRV_ERROR_FAILED_DEPENDENCIES;
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWGetTimelineObj(PVRSRV_TIMELINE iSWTimeline,
                                  SYNC_TIMELINE_OBJ *ppvSWTimelineObj)
{
	struct pvr_counting_fence_timeline *timeline = 
		pvr_sync_get_sw_timeline(iSWTimeline);

	if (timeline == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	*ppvSWTimelineObj = timeline;
	return PVRSRV_OK;
}

PVRSRV_ERROR SyncSWGetFenceObj(PVRSRV_FENCE iSWFence,
                               SYNC_FENCE_OBJ *ppvSWFenceObj)
{
	struct dma_fence *psFence = sync_file_get_fence(iSWFence);

	if(psFence == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	*ppvSWFenceObj = (SYNC_FENCE_OBJ*)psFence;
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
