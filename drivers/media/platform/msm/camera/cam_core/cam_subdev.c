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

#include "cam_subdev.h"
#include "cam_node.h"

/**
 * cam_subdev_subscribe_event()
 *
 * @brief: function to subscribe to v4l2 events
 *
 * @sd:                    Pointer to struct v4l2_subdev.
 * @fh:                    Pointer to struct v4l2_fh.
 * @sub:                   Pointer to struct v4l2_event_subscription.
 */
static int cam_subdev_subscribe_event(struct v4l2_subdev *sd,
	struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, CAM_SUBDEVICE_EVENT_MAX, NULL);
}

/**
 * cam_subdev_unsubscribe_event()
 *
 * @brief: function to unsubscribe from v4l2 events
 *
 * @sd:                    Pointer to struct v4l2_subdev.
 * @fh:                    Pointer to struct v4l2_fh.
 * @sub:                   Pointer to struct v4l2_event_subscription.
 */
static int cam_subdev_unsubscribe_event(struct v4l2_subdev *sd,
	struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static long cam_subdev_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
	void *arg)
{
	long rc;
	struct cam_node *node =
		(struct cam_node *) v4l2_get_subdevdata(sd);

	if (!node || node->state == CAM_NODE_STATE_UNINIT) {
		rc = -EINVAL;
		goto end;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_node_handle_ioctl(node,
			(struct cam_control *) arg);
		break;
	default:
		pr_err("Invalid command %d for %s!\n", cmd,
			node->name);
		rc = -EINVAL;
	}
end:
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_subdev_compat_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	return cam_subdev_ioctl(sd, cmd, compat_ptr(arg));
}
#endif

const struct v4l2_subdev_core_ops cam_subdev_core_ops = {
	.ioctl = cam_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_subdev_compat_ioctl,
#endif
	.subscribe_event = cam_subdev_subscribe_event,
	.unsubscribe_event = cam_subdev_unsubscribe_event,
};

static const struct v4l2_subdev_ops cam_subdev_ops = {
	.core = &cam_subdev_core_ops,
};

int cam_subdev_remove(struct cam_subdev *sd)
{
	if (!sd)
		return -EINVAL;

	cam_unregister_subdev(sd);
	cam_node_deinit((struct cam_node *)sd->token);
	kfree(sd->token);

	return 0;
}

int cam_subdev_probe(struct cam_subdev *sd, struct platform_device *pdev,
	char *name, uint32_t dev_type)
{
	int rc;
	struct cam_node *node = NULL;

	if (!sd || !pdev || !name) {
		rc = -EINVAL;
		goto err;
	}

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		rc = -ENOMEM;
		goto err;
	}

	/* Setup camera v4l2 subdevice */
	sd->pdev = pdev;
	sd->name = name;
	sd->ops = &cam_subdev_ops;
	sd->token = node;
	sd->sd_flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sd->ent_function = dev_type;
	rc = cam_register_subdev(sd);
	if (rc) {
		pr_err("%s: cam_register_subdev() failed for dev: %s!\n",
			__func__, sd->name);
		goto err;
	}
	platform_set_drvdata(sd->pdev, sd);
	return rc;
err:
	kfree(node);
	return rc;
}
