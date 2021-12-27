// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include "cam_subdev.h"
#include "cam_node.h"
#include "cam_debug_util.h"

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
		CAM_ERR(CAM_CORE, "Invalid command %d for %s", cmd,
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
	struct cam_control cmd_data;
	int rc;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_CORE, "Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}
	rc = cam_subdev_ioctl(sd, cmd, &cmd_data);
	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_CORE,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
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

	if (!sd || !pdev || !name)
		return -EINVAL;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

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
		CAM_ERR(CAM_CORE, "cam_register_subdev() failed for dev: %s",
			sd->name);
		goto err;
	}
	platform_set_drvdata(sd->pdev, sd);
	return rc;
err:
	kfree(node);
	return rc;
}
