/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SDE_RECOVERY_MANAGER_H__
#define __SDE_RECOVERY_MANAGER_H__

#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <drm/msm_drm.h>
#include <linux/slab.h>
#include <drm/drmP.h>

/* MSM Recovery Manager related definitions */
#define MAX_REC_NAME_LEN (16)

/* MSM Recovery Manager Event Code */
#define DRM_EVENT_SDE_UNDERRUN		0x80000008
#define DRM_EVENT_SDE_SMMUFAULT		0x80000009
#define DRM_EVENT_SDE_VSYNC_MISS	0x80000010
#define DRM_EVENT_RECOVERY_SUCCESS	0x80000011
#define DRM_EVENT_RECOVERY_FAILURE	0x80000012
#define DRM_EVENT_BRIDGE_ERROR		0x80000013

/*
 * struct sde_recovery_client - Client information
 * @name: name of the client.
 * @recovery_cb: recovery callback to recover the events reported.
 * @events: list of events that can be handled by client.
 * @num_events: number of events supported by the client.
 * @handle: opaque data to callback.
 * @list: link list
 */
struct sde_recovery_client {
	char name[MAX_REC_NAME_LEN];
	int (*recovery_cb)(void *handle, u32 event, struct drm_crtc *crtc);
	u32 *events;
	int num_events;
	void *handle;
	struct list_head list;
};

#if IS_ENABLED(CONFIG_SDE_RECOVERY_MANAGER)

/*
 * Send a event for a crtc or all crtc to the recovery manager.
 * @dev       DRM device
 * @event     event
 * @crtc      CRTC or NULL for all active CRTC
 *
 * Return value:
 *   0        Success
 *   -EEXIST  a same event for the same crtc has been queue and not processed
 *   -EINVAL  CRTC index out of the range
 *   -ENOMEM  out of memory
 */
int sde_recovery_set_event(struct drm_device *dev, u32 event,
		struct drm_crtc *crtc);

/*
 * Register a new client to recovery manager.
 * @dev       DRM device
 * @client    client information
 *
 * Return value:
 *   0        Success
 *   -EINVAL  client name is NULL
 *   -ENOENT  recovery manager hasn't been initialized
 */
int sde_recovery_client_register(struct drm_device *dev,
		struct sde_recovery_client *client);

/*
 * Unregister a existing client from recovery manager.
 * @dev       DRM device
 * @client    client information
 *
 * Return value:
 *   0        Success
 *   -EINVAL  client is NULL
 *   -ENOENT  recovery manager hasn't been initialized
 */
int sde_recovery_client_unregister(struct drm_device *dev,
		struct sde_recovery_client *client);

#else

static inline int sde_recovery_set_event(struct drm_device *dev, u32 event,
		struct drm_crtc *crtc)
{
	return 0;
}
static inline int sde_recovery_client_register(struct drm_device *dev,
		struct sde_recovery_client *client)
{
	return 0;
}
static inline int sde_recovery_client_unregister(struct drm_device *dev,
		struct sde_recovery_client *client)
{
	return 0;
}

#endif

#endif /* __SDE_RECOVERY_MANAGER_H__ */
