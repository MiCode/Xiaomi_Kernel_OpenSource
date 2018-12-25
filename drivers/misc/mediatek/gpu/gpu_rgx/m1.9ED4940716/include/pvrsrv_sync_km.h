/*************************************************************************/ /*!
@File
@Title         PVR synchronization interface
@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description   Types for server side code
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
#ifndef PVRSRV_SYNC_KM_H
#define PVRSRV_SYNC_KM_H
#if defined (__cplusplus)
extern "C" {
#endif

/*! Implementation independent types for passing fence/timeline to Services.
 */
typedef int32_t PVRSRV_FENCE;
typedef int32_t PVRSRV_TIMELINE;

/*! Maximum length for an annotation name string for fence sync model objects.
 */
#define PVRSRV_SYNC_NAME_LENGTH 32

/*! Possible states for a PVRSRV_FENCE */
typedef enum
{
    PVRSRV_FENCE_NOT_SIGNALLED,             /*!< fence has not yet signalled (not all components have signalled) */
    PVRSRV_FENCE_SIGNALLED                  /*!< fence has signalled (all components have signalled/errored) */
} PVRSRV_FENCE_STATE;

/* Typedefs for opaque pointers to implementation-specific structures
 */
typedef void *SYNC_TIMELINE_OBJ;
typedef void *SYNC_FENCE_OBJ;

/* Macros for Kick API callers using the fence sync model
 */
#define PVRSRV_NO_CHECK_FENCE_REQUIRED      -1   /*!< used when submitted work is not fenced (eg first kick) */
#define PVRSRV_NO_UPDATE_TIMELINE_REQUIRED  -1   /*!< used when caller does not want an update fence generated */
#define PVRSRV_NO_UPDATE_FENCE_REQUIRED     NULL /*!< used when caller does not want an update fence generated */

/* Macros for Kick API callers NOT using the fence sync model
 */
#define PVRSRV_FENCE_INTERFACE_UNUSED     -1     /*!< passed in Timeline and Fence values when interface is unused */
#define PVRSRV_FENCE_INTERFACE_PTR_UNUSED NULL   /*!< passed in update fence parameter when interface is unused */


/* PVRSRV Layer internal only
 * Negative references to timelines and fences are invalid.
 */
#define PVRSRV_TIMELINE_INVALID     -1
#define PVRSRV_FENCE_INVALID        -1

#if PVRSRV_NO_UPDATE_TIMELINE_REQUIRED != PVRSRV_TIMELINE_INVALID
#error "PVRSRV_NO_UPDATE_TIMELINE_REQUIRED must equal PVRSRV_TIMELINE_INVALID"
#endif

#if PVRSRV_NO_CHECK_FENCE_REQUIRED != PVRSRV_FENCE_INVALID
#error "PVRSRV_NO_CHECK_FENCE_REQUIRED must equal PVRSRV_FENCE_INVALID"
#endif

#if PVRSRV_FENCE_INTERFACE_UNUSED != PVRSRV_FENCE_INVALID
#error "PVRSRV_FENCE_INTERFACE_UNUSED must equal PVRSRV_FENCE_INVALID"
#endif


#if defined (__cplusplus)
}
#endif
#endif	/* PVRSRV_SYNC_KM_H */

