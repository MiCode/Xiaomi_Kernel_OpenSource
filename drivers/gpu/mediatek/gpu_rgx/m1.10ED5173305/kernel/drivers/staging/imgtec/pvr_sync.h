/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File           pvr_sync.h
@Title          Kernel driver for Android's sync mechanism
@Codingstyle    LinuxKernel
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

#ifndef _PVR_SYNC_H
#define _PVR_SYNC_H

#include <linux/device.h>
#include <linux/kref.h>

#include "pvr_fd_sync_kernel.h"


/* Services internal interface */

/**************************************************************************/ /*!
@Function       pvr_sync_init
@Description    Create an internal sync context
@Input          dev: Linux device
@Return         PVRSRV_OK on success
*/ /***************************************************************************/
enum PVRSRV_ERROR pvr_sync_init(struct device *dev);

/**************************************************************************/ /*!
@Function       pvr_sync_deinit
@Description    Destroy an internal sync context. Drains any work items with
				outstanding sync fence updates/dependencies.
@Input          None
@Return         None
*/ /***************************************************************************/
void pvr_sync_deinit(void);

struct _RGXFWIF_DEV_VIRTADDR_;
struct pvr_sync_append_data;

/**************************************************************************/ /*!
@Function       pvr_sync_get_updates
@Description    Internal API to resolve sync update data
@Input          sync_data: PVR sync data
@Output			nr_fences: number of UFO fence updates
@Output			ufo_addrs: UFO fence addresses
@Output			values: UFO fence values
@Return         None
*/ /***************************************************************************/
void pvr_sync_get_updates(const struct pvr_sync_append_data *sync_data,
	u32 *nr_fences,
	struct _RGXFWIF_DEV_VIRTADDR_ **ufo_addrs,
	u32 **values);

/**************************************************************************/ /*!
@Function       pvr_sync_get_checks
@Description    Internal API to resolve sync check data
@Input          sync_data: PVR sync data
@Output			nr_fences: number of UFO fence checks
@Output			ufo_addrs: UFO fence addresses
@Output			values: UFO fence values
@Return         None
*/ /***************************************************************************/
void pvr_sync_get_checks(const struct pvr_sync_append_data *sync_data,
	u32 *nr_fences,
	struct _RGXFWIF_DEV_VIRTADDR_ **ufo_addrs,
	u32 **values);

/**************************************************************************/ /*!
@Function       pvr_sync_rollback_append_fences
@Description    Undo the last sync fence and its timeline if present
				Must be called before pvr_sync_free_append_fences_data which may
				free the fence sync object.
@Input          sync_data: PVR sync data
@Return         None
*/ /***************************************************************************/
void pvr_sync_rollback_append_fences(struct pvr_sync_append_data *sync_data);

/**************************************************************************/ /*!
@Function       pvr_sync_nohw_complete_fences
@Description    Force updates to progress sync timeline when hardware is not present.
@Input          sync_data: PVR sync data
@Return         None
*/ /***************************************************************************/
void pvr_sync_nohw_complete_fences(struct pvr_sync_append_data *sync_data);

/**************************************************************************/ /*!
@Function       pvr_sync_free_append_fences_data
@Description    Commit the sync fences/updates
@Input          sync_data: PVR sync data
@Return         None
*/ /***************************************************************************/
void pvr_sync_free_append_fences_data(struct pvr_sync_append_data *sync_data);

/**************************************************************************/ /*!
@Function       pvr_sync_get_update_fd
@Description    Get the file descriptor for the sync fence updates
@Input          sync_data: PVR sync data
@Return         Valid sync file descriptor on success; -EINVAL on failure
*/ /***************************************************************************/
int pvr_sync_get_update_fd(struct pvr_sync_append_data *sync_data);

/**************************************************************************/ /*!
@Function       pvr_sync_get_sw_timeline
@Description    Get the PVR sync timeline from its file descriptor
@Input          fd: Linux file descriptor
@Return         PVR sync timeline
*/ /***************************************************************************/
struct pvr_counting_fence_timeline;
struct pvr_counting_fence_timeline *pvr_sync_get_sw_timeline(int fd);

/* PVR sync 2 SW timeline interface */

struct pvr_sw_sync_timeline {
	/* sw_sync_timeline must come first to allow casting of a ptr */
	/* to the wrapping struct to a ptr to the sw_sync_timeline    */
	struct sw_sync_timeline *sw_sync_timeline;
	u64 current_value;
	u64 next_value;
	/* Reference count for this object */
	struct kref kref;
};

/**************************************************************************/ /*!
@Function       pvr_sw_sync_release_timeline
@Description    Release the current reference on a PVR SW sync timeline
@Input          timeline: the PVR SW sync timeline
@Return         None
*/ /***************************************************************************/
void pvr_sw_sync_release_timeline(struct pvr_sw_sync_timeline *timeline);

#endif /* _PVR_SYNC_H */
