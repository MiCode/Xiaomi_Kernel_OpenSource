#ifndef _SYNC_FALLBACK_SERVER_H_
#define _SYNC_FALLBACK_SERVER_H_

#include "img_types.h"
#include "sync_checkpoint.h"
#include "device.h"


typedef struct _PVRSRV_TIMELINE_SERVER_ PVRSRV_TIMELINE_SERVER;
typedef struct _PVRSRV_FENCE_SERVER_ PVRSRV_FENCE_SERVER;

typedef struct _PVRSRV_SYNC_PT_ PVRSRV_SYNC_PT;

#define SYNC_FB_TIMELINE_MAX_LENGTH PVRSRV_SYNC_NAME_LENGTH
#define SYNC_FB_FENCE_MAX_LENGTH PVRSRV_SYNC_NAME_LENGTH

/*****************************************************************************/
/*                                                                           */
/*                         SW SPECIFIC FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/

PVRSRV_ERROR SyncFbTimelineCreateSW(IMG_UINT32 uiTimelineNameSize,
                                    const IMG_CHAR *pszTimelineName,
                                    PVRSRV_TIMELINE_SERVER **ppsTimeline);

/*****************************************************************************/
/*                                                                           */
/*                         PVR SPECIFIC FUNCTIONS                            */
/*                                                                           */
/*****************************************************************************/

PVRSRV_ERROR SyncFbTimelineCreatePVR(IMG_UINT32 uiTimelineNameSize,
                                     const IMG_CHAR *pszTimelineName,
                                     PVRSRV_TIMELINE_SERVER **ppsTimeline);

PVRSRV_ERROR SyncFbFenceCreatePVR(const IMG_CHAR *pszName,
                                  PVRSRV_TIMELINE iTl,
                                  PSYNC_CHECKPOINT_CONTEXT hSyncCheckpointContext,
                                  PVRSRV_FENCE *piOutFence,
                                  IMG_UINT32 *puiFenceUID,
                                  void **ppvFenceFinaliseData,
                                  PSYNC_CHECKPOINT *ppsOutCheckpoint,
                                  void **ppvTimelineUpdateSync,
                                  IMG_UINT32 *puiTimelineUpdateValue);

PVRSRV_ERROR SyncFbFenceResolvePVR(PSYNC_CHECKPOINT_CONTEXT psContext,
                                   PVRSRV_FENCE iFence,
                                   IMG_UINT32 *puiNumCheckpoints,
                                   PSYNC_CHECKPOINT **papsCheckpoints,
                                   IMG_UINT32 *puiFenceUID);

/*****************************************************************************/
/*                                                                           */
/*                         GENERIC FUNCTIONS                                 */
/*                                                                           */
/*****************************************************************************/

PVRSRV_ERROR SyncFbTimelineRelease(PVRSRV_TIMELINE_SERVER *psTl);

PVRSRV_ERROR SyncFbFenceRelease(PVRSRV_FENCE_SERVER *psFence);

PVRSRV_ERROR SyncFbFenceDup(PVRSRV_FENCE_SERVER *psInFence,
                            PVRSRV_FENCE_SERVER **ppsOutFence);

PVRSRV_ERROR SyncFbFenceMerge(PVRSRV_FENCE_SERVER *psInFence1,
                              PVRSRV_FENCE_SERVER *psInFence2,
                              IMG_UINT32 uiFenceNameSize,
                              const IMG_CHAR *pszFenceName,
                              PVRSRV_FENCE_SERVER **ppsOutFence);

PVRSRV_ERROR SyncFbFenceWait(PVRSRV_FENCE_SERVER *psFence,
                             IMG_UINT32 uiTimeout);

PVRSRV_ERROR SyncFbFenceDump(PVRSRV_FENCE_SERVER *psFence,
                             IMG_UINT32 uiLine,
                             IMG_UINT32 uiFileNameLenght,
                             const IMG_CHAR *pszFile);

PVRSRV_ERROR SyncFbRegisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode);

PVRSRV_ERROR SyncFbDeregisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode);
#endif /* _SYNC_FALLBACK_SERVER_H_ */
