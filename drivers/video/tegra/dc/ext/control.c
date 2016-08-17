/*
 * drivers/video/tegra/dc/ext/control.c
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION, All rights reserved.
 *
 * Author: Robert Morell <rmorell@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "tegra_dc_ext_priv.h"

static struct tegra_dc_ext_control g_control;

int tegra_dc_ext_process_hotplug(int output)
{
	return tegra_dc_ext_queue_hotplug(&g_control, output);
}

static int
get_output_properties(struct tegra_dc_ext_control_output_properties *properties)
{
	struct tegra_dc *dc;

	/* TODO: this should be more dynamic */
	if (properties->handle > 2)
		return -EINVAL;

	properties->associated_head = properties->handle;
	properties->head_mask = (1 << properties->associated_head);

	dc = tegra_dc_get_dc(properties->associated_head);
	switch (tegra_dc_get_out(dc)) {
	case TEGRA_DC_OUT_DSI:
		properties->type = TEGRA_DC_EXT_DSI;
		break;
	case TEGRA_DC_OUT_RGB:
		properties->type = TEGRA_DC_EXT_LVDS;
		break;
	case TEGRA_DC_OUT_HDMI:
		properties->type = TEGRA_DC_EXT_HDMI;
		break;
	default:
		return -EINVAL;
	}
	properties->connected = tegra_dc_get_connected(dc);

	return 0;
}

static int get_output_edid(struct tegra_dc_ext_control_output_edid *edid)
{
	struct tegra_dc *dc;
	size_t user_size = edid->size;
	struct tegra_dc_edid *dc_edid = NULL;
	int ret = 0;

	/* TODO: this should be more dynamic */
	if (edid->handle > 2)
		return -EINVAL;

	dc = tegra_dc_get_dc(edid->handle);

	dc_edid = tegra_dc_get_edid(dc);
	if (IS_ERR(dc_edid))
		return PTR_ERR(dc_edid);

	if (!dc_edid) {
		edid->size = 0;
	} else {
		edid->size = dc_edid->len;

		if (user_size < edid->size) {
			ret = -EFBIG;
			goto done;
		}

		if (copy_to_user(edid->data, dc_edid->buf, edid->size)) {
			ret = -EFAULT;
			goto done;
		}

	}

done:
	if (dc_edid)
		tegra_dc_put_edid(dc_edid);

	return ret;
}

static int set_event_mask(struct tegra_dc_ext_control_user *user, u32 mask)
{
	struct list_head *list, *tmp;

	if (mask & ~TEGRA_DC_EXT_EVENT_MASK_ALL)
		return -EINVAL;

	mutex_lock(&user->lock);

	user->event_mask = mask;

	list_for_each_safe(list, tmp, &user->event_list) {
		struct tegra_dc_ext_event_list *ev_list;
		ev_list = list_entry(list, struct tegra_dc_ext_event_list,
			list);
		if (!(mask & ev_list->event.type)) {
			list_del(list);
			kfree(ev_list);
		}
	}
	mutex_unlock(&user->lock);

	return 0;
}

static int get_capabilities(struct tegra_dc_ext_control_capabilities *caps)
{
	caps->caps = TEGRA_DC_EXT_CAPABILITIES;
	return 0;
}

static long tegra_dc_ext_control_ioctl(struct file *filp, unsigned int cmd,
				       unsigned long arg)
{
	void __user *user_arg = (void __user *)arg;
	struct tegra_dc_ext_control_user *user = filp->private_data;

	switch (cmd) {
	case TEGRA_DC_EXT_CONTROL_GET_NUM_OUTPUTS:
	{
		u32 num = tegra_dc_ext_get_num_outputs();

		if (copy_to_user(user_arg, &num, sizeof(num)))
			return -EFAULT;

		return 0;
	}
	case TEGRA_DC_EXT_CONTROL_GET_OUTPUT_PROPERTIES:
	{
		struct tegra_dc_ext_control_output_properties args;
		int ret;

		if (copy_from_user(&args, user_arg, sizeof(args)))
			return -EFAULT;

		ret = get_output_properties(&args);

		if (copy_to_user(user_arg, &args, sizeof(args)))
			return -EFAULT;

		return ret;
	}
	case TEGRA_DC_EXT_CONTROL_GET_OUTPUT_EDID:
	{
		struct tegra_dc_ext_control_output_edid args;
		int ret;

		if (copy_from_user(&args, user_arg, sizeof(args)))
			return -EFAULT;

		ret = get_output_edid(&args);

		if (copy_to_user(user_arg, &args, sizeof(args)))
			return -EFAULT;

		return ret;
	}
	case TEGRA_DC_EXT_CONTROL_SET_EVENT_MASK:
		return set_event_mask(user, (u32) arg);
	case TEGRA_DC_EXT_CONTROL_GET_CAPABILITIES:
	{
		struct tegra_dc_ext_control_capabilities args;
		int ret;

		ret = get_capabilities(&args);

		if (copy_to_user(user_arg, &args, sizeof(args)))
			return -EFAULT;

		return ret;
	}
	default:
		return -EINVAL;
	}
}

static int tegra_dc_ext_control_open(struct inode *inode, struct file *filp)
{
	struct tegra_dc_ext_control_user *user;
	struct tegra_dc_ext_control *control;

	user = kzalloc(sizeof(*user), GFP_KERNEL);
	if (!user)
		return -ENOMEM;

	control = container_of(inode->i_cdev, struct tegra_dc_ext_control,
		cdev);
	user->control = control;;

	INIT_LIST_HEAD(&user->event_list);
	mutex_init(&user->lock);

	filp->private_data = user;

	mutex_lock(&control->lock);
	list_add(&user->list, &control->users);
	mutex_unlock(&control->lock);

	return 0;
}

static int tegra_dc_ext_control_release(struct inode *inode, struct file *filp)
{
	struct tegra_dc_ext_control_user *user = filp->private_data;
	struct tegra_dc_ext_control *control = user->control;

	/* This will free any pending events for this user */
	set_event_mask(user, 0);

	mutex_lock(&control->lock);
	list_del(&user->list);
	mutex_unlock(&control->lock);

	kfree(user);

	return 0;
}

static const struct file_operations tegra_dc_ext_event_devops = {
	.owner =		THIS_MODULE,
	.open =			tegra_dc_ext_control_open,
	.release =		tegra_dc_ext_control_release,
	.read =			tegra_dc_ext_event_read,
	.poll =			tegra_dc_ext_event_poll,
	.unlocked_ioctl =	tegra_dc_ext_control_ioctl,
};

int tegra_dc_ext_control_init(void)
{
	struct tegra_dc_ext_control *control = &g_control;
	int ret;

	cdev_init(&control->cdev, &tegra_dc_ext_event_devops);
	control->cdev.owner = THIS_MODULE;
	ret = cdev_add(&control->cdev, tegra_dc_ext_devno, 1);
	if (ret)
		return ret;

	control->dev = device_create(tegra_dc_ext_class,
	     NULL, tegra_dc_ext_devno, NULL, "tegra_dc_ctrl");
	if (IS_ERR(control->dev)) {
		ret = PTR_ERR(control->dev);
		cdev_del(&control->cdev);
	}

	mutex_init(&control->lock);

	INIT_LIST_HEAD(&control->users);

	return ret;
}
