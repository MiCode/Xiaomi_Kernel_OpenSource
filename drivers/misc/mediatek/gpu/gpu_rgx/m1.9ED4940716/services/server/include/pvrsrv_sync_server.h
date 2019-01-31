#ifndef _PVRSRV_SYNC_SERVER_H_
#define _PVRSRV_SYNC_SERVER_H_

#include "img_types.h"
#include "pvrsrv_sync_km.h"

#define SYNC_SW_TIMELINE_MAX_LENGTH PVRSRV_SYNC_NAME_LENGTH
#define SYNC_SW_FENCE_MAX_LENGTH PVRSRV_SYNC_NAME_LENGTH

/*****************************************************************************/
/*                                                                           */
/*                      SW TIMELINE SPECIFIC FUNCTIONS                       */
/*                                                                           */
/*****************************************************************************/

PVRSRV_ERROR SyncSWTimelineFenceCreateKM(PVRSRV_TIMELINE iSWTimeline,
                                        IMG_UINT32 ui32NextSyncPtVal,
                                        const IMG_CHAR *pszFenceName,
                                        PVRSRV_FENCE *piOutputFence);

PVRSRV_ERROR SyncSWTimelineAdvanceKM(SYNC_TIMELINE_OBJ pvSWTimelineObj);

PVRSRV_ERROR SyncSWTimelineReleaseKM(SYNC_TIMELINE_OBJ pvSWTimelineObj);

PVRSRV_ERROR SyncSWTimelineFenceReleaseKM(SYNC_FENCE_OBJ pvSWFenceObj);

PVRSRV_ERROR SyncSWTimelineFenceWaitKM(SYNC_FENCE_OBJ pvSWFenceObj,
                                       IMG_UINT32 uiTimeout);

PVRSRV_ERROR SyncSWGetTimelineObj(PVRSRV_TIMELINE iSWTimeline,
                                  SYNC_TIMELINE_OBJ *ppvSWTimelineObj);

PVRSRV_ERROR SyncSWGetFenceObj(PVRSRV_FENCE iSWFence,
                               SYNC_FENCE_OBJ *ppvSWFenceObj);

IMG_BOOL PVRSRVIsTimelineValidKM(PVRSRV_TIMELINE iTimeline);
IMG_BOOL PVRSRVIsFenceValidKM(PVRSRV_FENCE iFence);

#endif /* _PVRSRV_SYNC_SERVER_H_ */
