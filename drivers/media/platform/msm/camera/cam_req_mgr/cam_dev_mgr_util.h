/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CAM_DEV_MGR_UTIL_H_
#define _CAM_DEV_MGR_UTIL_H_

#define CAM_SUBDEVICE_EVENT_MAX 30

#include <linux/types.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>

/**
 * struct cam_subdev - describes a camera sub-device
 *
 * @sd: struct v4l2_subdev
 * @ops: struct v4l2_subdev_ops
 * @internal_ops: struct v4l2_subdev_internal_ops
 * @name: Name of the sub-device. Please notice that the name must be unique.
 * @sd_flags: subdev flags. Can be:
 *   %V4L2_SUBDEV_FL_HAS_DEVNODE - Set this flag if this subdev needs a
 *   device node;
 *   %V4L2_SUBDEV_FL_HAS_EVENTS -  Set this flag if this subdev generates
 *   events.
 * @token: pointer to cookie of the client driver
 * @ent_function: media entity function type. Can be:
 *   %CAM_IFE_DEVICE_TYPE - identifies as IFE device;
 *   %CAM_ICP_DEVICE_TYPE - identifies as ICP device.
 * Each instance of a subdev driver should create this struct, either
 * stand-alone or embedded in a larger struct.
 *
 * This structure should be initialized/registered by cam_register_subdev
 */
struct cam_subdev {
	struct v4l2_subdev sd;
	const struct v4l2_subdev_ops *ops;
	const struct v4l2_subdev_internal_ops *internal_ops;
	char *name;
	u32 sd_flags;
	void *token;
	u32 ent_function;
};

/**
 * cam_register_subdev()
 *
 * @brief:  Registration function for camera subdevice
 *
 * @sd: pointer to struct cam_subdev.
 */
int cam_register_subdev(struct cam_subdev *sd);

/**
 * cam_unregister_subdev()
 *
 * @brief:  Unregistration function for camera subdevice
 *
 * @sd: pointer to struct cam_subdev.
 */
int cam_unregister_subdev(struct cam_subdev *sd);

/**
 * cam_send_event()
 *
 * @brief: Inline function to sent event to user space
 *
 * @csd: pointer to struct cam_subdev.
 * @ev: pointer to struct v4l2_event.
 */
static inline int cam_send_event(struct cam_subdev *csd,
	const struct v4l2_event *ev)
{
	if (!csd || !ev)
		return -EINVAL;

	v4l2_event_queue(csd->sd.devnode, ev);

	return 0;
}

/**
 * cam_get_subdev_data()
 *
 * @brief: Inline function to retrieve the private data
 *
 * @csd: pointer to struct cam_subdev.
 */
static inline void *cam_get_subdev_data(struct cam_subdev *csd)
{
	if (!csd)
		return ERR_PTR(-EINVAL);

	return v4l2_get_subdevdata(&csd->sd);
}

/**
 * cam_sd_subscribe_event()
 *
 * @brief: Inline function to subscribe to v4l2 events
 *
 * @sd: pointer to struct v4l2_subdev.
 * @fh: pointer to struct v4l2_fh.
 * @sub: pointer to struct v4l2_event_subscription.
 */
static inline int cam_sd_subscribe_event(struct v4l2_subdev *sd,
	struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, CAM_SUBDEVICE_EVENT_MAX, NULL);
}

/**
 * cam_sd_unsubscribe_event()
 *
 * @brief: Inline function to unsubscribe from v4l2 events
 *
 * @sd: pointer to struct v4l2_subdev.
 * @fh: pointer to struct v4l2_fh.
 * @sub: pointer to struct v4l2_event_subscription.
 */
static inline int cam_sd_unsubscribe_event(struct v4l2_subdev *sd,
	struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}
#endif /* _CAM_DEV_MGR_UTIL_H_ */
