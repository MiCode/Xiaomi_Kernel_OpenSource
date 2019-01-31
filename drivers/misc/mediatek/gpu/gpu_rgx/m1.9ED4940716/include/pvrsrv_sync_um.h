/*************************************************************************/ /*!
@File
@Title         PVR synchronisation interface
@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description   API for synchronisation functions for client side code
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
#include "img_types.h"
#include "pvrsrv_error.h"
#include "pvr_debug.h"
#include "pvrsrv_sync_km.h"

#ifndef PVRSRV_SYNC_UM_H
#define PVRSRV_SYNC_UM_H
#if defined (__cplusplus)
extern "C" {
#endif


/*************************************************************************/ /*!
@Function       PVRSRVSyncInit

@Description    Initialise the synchronisation code for the calling
                process. This has to be called before any other functions in
                this synchronisation module.

@Return         PVRSRV_OK if the initialisation was successful
                PVRSRV_ERROR_xxx if an error occurred
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVSyncInitI(void);
#define PVRSRVSyncInit(void) \
		PVRSRVSyncInitI(void)

/*************************************************************************/ /*!
@Function       PVRSRVSyncDeinit

@Description    Deinitialises the synchronisation code for the calling process.
                This has to be called after successful initialisation and after
                any other call of this module has finished.

@Return         PVRSRV_OK if the deinitialisation was successful
                PVRSRV_ERROR_xxx if an error occurred
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVSyncDeinitI(void);
#define PVRSRVSyncDeinit(void) \
		PVRSRVSyncDeinitI(void)


/*************************************************************************/ /*!
@Function       PVRSRVTimelineCreate

@Description    Allocate a new synchronisation timeline.
                The timeline value is initialised to zero.

@Input          pszTimelineName     String to be used to annotate timeline
                                    (for debug)

@Output         hTimeline           Handle to created timeline

@Return         PVRSRV_OK if the timeline was successfully created
                PVRSRV_ERROR_NOMEM if there was insufficient memory to
                    create the new timeline
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVTimelineCreateI(PVRSRV_TIMELINE   *phTimeline,
                      const IMG_CHAR    *pszTimelineName
                      PVR_DBG_FILELINE_PARAM);
#define PVRSRVTimelineCreate(hTimeline, pszTimelineName) \
    PVRSRVTimelineCreateI( (hTimeline), (pszTimelineName) \
                           PVR_DBG_FILELINE)

/*************************************************************************/ /*!
@Function       PVRSRVTimelineDestroy

@Description    Destroy a timeline
                If the timeline has no outstanding checks or updates on
                it then it will be destroyed immediately.
                If there are outstanding checks or updates, the timeline
                will be flagged for destruction once all outstanding
                checks and updates are destroyed.
                A timeline marked for destruction may not have further
                checks or updates created for it.

@Input          hTimeline            The timeline to destroy

@Return         PVRSRV_OK if a valid active timeline was specified
                PVRSRV_ERROR_INVALID_PARAMS if an unrecognised timeline
                    was specified
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVTimelineDestroyI(PVRSRV_TIMELINE hTimeline
                       PVR_DBG_FILELINE_PARAM);
#define PVRSRVTimelineDestroy(hTimeline) \
    PVRSRVTimelineDestroyI( (hTimeline) PVR_DBG_FILELINE)

/*************************************************************************/ /*!
@Function       PVRSRVSWTimelineCreate

@Description    Allocate a new software timeline for synchronisation.
                Software timelines are different to timelines created with
                PVRSRVTimelineCreate in that they represent a strictly ordered
                sequence of events *progressed on the CPU* rather than the GPU

                The sequence of events has to be modelled by the application
                itself:
                1. First the application creates a SW timeline (this call)
                2. After creating some workload on the CPU the application can
                create a fence for it by calling PVRSRVSWFenceCreate and pass
                in a software timeline.
                3. When the workload finished and the application wants
                to signal potential waiters that work has finished, it can call
                PVRSRVSWTimelineAdvance which will signal the oldest fence
                on this software timeline

                Destroy with PVRSRVTimelineDestroy

@Input          pszSWTimelineName    String to be used to annotate the software
                                     timeline (for debug)

@Output         phSWTimeline         Handle to created software timeline

@Return         PVRSRV_OK if the timeline was successfully created
                PVRSRV_ERROR_xxx if an error occurred
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVSWTimelineCreateI(PVRSRV_TIMELINE   *phSWTimeline,
                        const IMG_CHAR    *pszSWTimelineName
                        PVR_DBG_FILELINE_PARAM);
#define PVRSRVSWTimelineCreate(hSWTimeline, pszSWTimelineName) \
    PVRSRVSWTimelineCreateI( (hSWTimeline), (pszSWTimelineName) \
                              PVR_DBG_FILELINE)

/*************************************************************************/ /*!
@Function       PVRSRVFenceWait

@Description    Wait for a fence checkpoint to be signalled.

@Input          hFence              Handle to the fence

@Input          ui32TimeoutInMs     Maximum time to wait (in milliseconds)

@Return         PVRSRV_OK once the fence has been passed (all component
                    checkpoints have either signalled or errored)
                PVRSRV_ERROR_TIMEOUT if the poll has exceeded the timeout
                PVRSRV_ERROR_INVALID_PARAMS if an unrecognised fence was specified
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVFenceWaitI(PVRSRV_FENCE hFence,
                 IMG_UINT32 ui32TimeoutInMs
                 PVR_DBG_FILELINE_PARAM);
#define PVRSRVFenceWait(hFence, ui32TimeoutInMs) \
    PVRSRVFenceWaitI( (hFence), (ui32TimeoutInMs) \
                      PVR_DBG_FILELINE)

/*************************************************************************/ /*!
@Function       PVRSRVFenceDup

@Description    Create a duplicate of the specified fence.
                The original fence will remain unchanged.
                The new fence will be an exact copy of the original and
                will reference the same timeline checkpoints as the
                source fence at the time of its creation.
                Any OSNativeSyncs attached to the original fence will also
                be attached to the duplicated fence.
                NB. If the source fence is subsequently merged or deleted
                    it will then differ from the dup'ed copy (which will
                    be unaffected).

@Input          hSourceFence        Handle of the fence to be duplicated

@Output         phOutputFence       Handle of newly created duplicate fence

@Return         PVRSRV_OK if the duplicate fence was successfully created
                PVRSRV_ERROR_INVALID_PARAMS if an unrecognised fence was specified
                PVRSRV_ERROR_NOMEM if there was insufficient memory to create
                    the new fence
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVFenceDupI(PVRSRV_FENCE hSourceFence,
                PVRSRV_FENCE *phOutputFence
                PVR_DBG_FILELINE_PARAM);
#define PVRSRVFenceDup(hSourceFence, phOutputFence) \
    PVRSRVFenceDupI( (hSourceFence), (phOutputFence) \
                      PVR_DBG_FILELINE)


/*************************************************************************/ /*!
@Function       PVRSRVFenceMerge

@Description    Merges two fences to create a new third fence.
                The original fences will remain unchanged.
                The new fence will be merge of two original fences and
                will reference the same timeline checkpoints as the
                two source fences with the exception that where each
                source fence contains a checkpoint for the same timeline
                the output fence will only contain the later of the two
                checkpoints.
                Any OSNativeSyncs attached to the original fences will also
                be attached to the resultant merged fence.
                If only one of the two source fences is valid, the function
                shall simply return a duplicate of the valid fence with no
                error indicated.
                NB. If the source fences are subsequently merged or deleted
                    they will then differ from the merged copy (which will
                    be unaffected).

@Input          hSourceFence1       Handle of the 1st fence to be merged

@Input          hSourceFence2       Handle of the 2nd fence to be merged

@Input          pszFenceName        Name of the created merged fence

@Output         phOutputFence       Handle of the newly created merged fence

@Return         PVRSRV_OK if the merged fence was successfully created
                PVRSRV_ERROR_INVALID_PARAMS if both source fences are unrecognised
                PVRSRV_ERROR_NOMEM if there was insufficient memory to create
                    the new merged fence
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVFenceMergeI(PVRSRV_FENCE hSourceFence1,
                  PVRSRV_FENCE hSourceFence2,
                  const IMG_CHAR *pszFenceName,
                  PVRSRV_FENCE *phOutputFence
                  PVR_DBG_FILELINE_PARAM);
#define PVRSRVFenceMerge(hSourceFence1, hSourceFence2, pszFenceName, phOutputFence) \
    PVRSRVFenceMergeI( (hSourceFence1), (hSourceFence2), (pszFenceName), (phOutputFence) \
                       PVR_DBG_FILELINE)


/*************************************************************************/ /*!
@Function       PVRSRVFenceAccumulate

@Description    Same as PVRSRVFenceMerge but destroys the input fences.
                If only one of the two source fences is valid, the function
                shall simply return the valid fence rather than performing
                a merge.

@Input          hSourceFence1       Handle of the 1st fence to be accumulated

@Input          hSourceFence2       Handle of the 2nd fence to be accumulated

@Input          pszFenceName        Name of the created accumulated fence

@Output         phOutputFence       Handle of the newly created fence

@Return         PVRSRV_OK if the accumulated fence was successfully created or
                    returned
                PVRSRV_ERROR_INVALID_PARAMS if both source fences are unrecognised
                PVRSRV_ERROR_NOMEM if there was insufficient memory to create
                    the new merged fence
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVFenceAccumulateI(PVRSRV_FENCE hSourceFence1,
                       PVRSRV_FENCE hSourceFence2,
                       const IMG_CHAR *pszFenceName,
                       PVRSRV_FENCE *phOutputFence
                       PVR_DBG_FILELINE_PARAM);
#define PVRSRVFenceAccumulate(hSourceFence1, hSourceFence2, pszFenceName, phOutputFence) \
    PVRSRVFenceAccumulateI( (hSourceFence1), (hSourceFence2), (pszFenceName), (phOutputFence) \
                            PVR_DBG_FILELINE)


/*************************************************************************/ /*!
@Function       PVRSRVFenceDump

@Description    Dumps debug information about the specified fence.

@Input          hFence              Handle to the fence

@Return         PVRSRV_OK if a valid fence was specified
                PVRSRV_ERROR_INVALID_PARAMS if an invalid fence was specified
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVFenceDumpI(PVRSRV_FENCE hFence
                 PVR_DBG_FILELINE_PARAM);
#define PVRSRVFenceDump(hFence) \
    PVRSRVFenceDumpI( (hFence) PVR_DBG_FILELINE)

/*************************************************************************/ /*!
@Function       PVRSRVFenceDestroy

@Description    Destroy a fence.
                The fence will be destroyed immediately if its refCount
                is now 0, otherwise it will be destroyed once all references
                to it have been dropped.

@Input          hFence              Handle to the fence

@Return         PVRSRV_OK if the fence was successfully marked for
                    destruction (now or later)
                PVRSRV_ERROR_INVALID_PARAMS if an unrecognised fence was specified
*/
/*****************************************************************************/
IMG_EXPORT PVRSRV_ERROR
PVRSRVFenceDestroyI(PVRSRV_FENCE hFence
                    PVR_DBG_FILELINE_PARAM);
#define PVRSRVFenceDestroy(hFence) \
    PVRSRVFenceDestroyI( (hFence) PVR_DBG_FILELINE)

/*************************************************************************/ /*!
@Function       PVRSRVIsTimelineValid

@Description    Checks whether the passed timeline handle has been set to an
                invalid value.
                Used to find out if the timeline can be passed into fence
                sync functions.

@Return         IMG_TRUE if the passed timeline is valid, IMG_FALSE if invalid
*/
/*****************************************************************************/
IMG_BOOL PVRSRVIsTimelineValid(PVRSRV_TIMELINE iTimeline);

/*************************************************************************/ /*!
@Function       PVRSRVIsFenceValid

@Description    Checks whether the passed fence handle has been set to an
                invalid value.
                Used to find out if the fence can be passed into fence
                sync functions.

@Return         IMG_TRUE if the passed fence is valid, IMG_FALSE if invalid
*/
/*****************************************************************************/
IMG_BOOL PVRSRVIsFenceValid(PVRSRV_FENCE iFence);


#if defined (__cplusplus)
}
#endif
#endif	/* PVRSRV_SYNC_UM_H */
