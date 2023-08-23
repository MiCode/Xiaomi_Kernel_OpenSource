/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_SUBDEV_H_
#define _CAM_SUBDEV_H_

#include <linux/types.h>
#include <linux/platform_device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>

#define CAM_SUBDEVICE_EVENT_MAX 30

/**
 * struct cam_subdev - describes a camera sub-device
 *
 * @pdev:                  Pointer to the platform device
 * @sd:                    V4l2 subdevice
 * @ops:                   V4l2 subdecie operations
 * @internal_ops:          V4l2 subdevice internal operations
 * @name:                  Name of the sub-device. Please notice that the name
 *                             must be unique.
 * @sd_flags:              Subdev flags. Can be:
 *                             %V4L2_SUBDEV_FL_HAS_DEVNODE - Set this flag if
 *                                 this subdev needs a device node.
 *                             %V4L2_SUBDEV_FL_HAS_EVENTS -  Set this flag if
 *                                 this subdev generates events.
 * @token:                 Pointer to cookie of the client driver
 * @ent_function:          Media entity function type. Can be:
 *                             %CAM_IFE_DEVICE_TYPE - identifies as IFE device.
 *                             %CAM_ICP_DEVICE_TYPE - identifies as ICP device.
 *
 * Each instance of a subdev driver should create this struct, either
 * stand-alone or embedded in a larger struct. This structure should be
 * initialized/registered by cam_register_subdev
 *
 */
struct cam_subdev {
	struct platform_device                *pdev;
	struct v4l2_subdev                     sd;
	const struct v4l2_subdev_ops          *ops;
	const struct v4l2_subdev_internal_ops *internal_ops;
	char                                  *name;
	u32                                    sd_flags;
	void                                  *token;
	u32                                    ent_function;
};

/**
 * cam_subdev_probe()
 *
 * @brief:      Camera Subdevice node probe function for v4l2 setup
 *
 * @sd:                    Camera subdevice object
 * @name:                  Name of the subdevice node
 * @dev_type:              Subdevice node type
 *
 */
int cam_subdev_probe(struct cam_subdev *sd, struct platform_device *pdev,
	char *name, uint32_t dev_type);

/**
 * cam_subdev_remove()
 *
 * @brief:      Called when subdevice node is unloaded
 *
 * @sd:                    Camera subdevice node object
 *
 */
int cam_subdev_remove(struct cam_subdev *sd);

/**
 * cam_register_subdev_fops()
 *
 * @brief:   This common utility function assigns subdev ops
 *
 * @fops:    v4l file operations
 */
void cam_register_subdev_fops(struct v4l2_file_operations *fops);

/**
 * cam_register_subdev()
 *
 * @brief:   This is the common utility function to be called by each camera
 *           subdevice node when it tries to register itself to the camera
 *           request manager
 *
 * @sd:                    Pointer to struct cam_subdev.
 */
int cam_register_subdev(struct cam_subdev *sd);

/**
 * cam_unregister_subdev()
 *
 * @brief:    This is the common utility function to be called by each camera
 *            subdevice node when it tries to unregister itself from the
 *            camera request manger
 *
 * @sd:                    Pointer to struct cam_subdev.
 */
int cam_unregister_subdev(struct cam_subdev *sd);

#endif /* _CAM_SUBDEV_H_ */
