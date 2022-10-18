// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */
#include <linux/err.h>
#include <linux/slab.h>
#include "msm_cooling_device.h"

#define BRIGHTNESS_CDEV_MAX 255

static int sde_cdev_get_max_brightness(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	*state = 200;

	return 0;
}

static int sde_cdev_get_cur_brightness(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct sde_cdev *disp_cdev = (struct sde_cdev *)cdev->devdata;

	*state = disp_cdev->thermal_state;

	return 0;
}

static int sde_cdev_set_cur_brightness(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	struct sde_cdev *disp_cdev = (struct sde_cdev *)cdev->devdata;
	unsigned long brightness_lvl = 0;

	if (state == 0 || state > 200)
		return -EINVAL;

	brightness_lvl = disp_cdev->bd->props.max_brightness * state / 200;
	if (brightness_lvl == disp_cdev->thermal_state)
		return 0;
	disp_cdev->thermal_state = brightness_lvl;
	blocking_notifier_call_chain(&disp_cdev->notifier_head,
					brightness_lvl, (void *)disp_cdev->bd);

	return 0;
}

static struct thermal_cooling_device_ops sde_cdev_ops = {
	.get_max_state = sde_cdev_get_max_brightness,
	.get_cur_state = sde_cdev_get_cur_brightness,
	.set_cur_state = sde_cdev_set_cur_brightness,
};

struct sde_cdev *backlight_cdev_register(struct device *dev,
					struct backlight_device *bd,
					struct notifier_block *n)
{
	struct sde_cdev *disp_cdev = NULL;

	if (!dev || !bd || !n)
		return ERR_PTR(-EINVAL);
	if (!of_find_property(dev->of_node, "#cooling-cells", NULL))
		return ERR_PTR(-ENODEV);

	disp_cdev = devm_kzalloc(dev, sizeof(*disp_cdev), GFP_KERNEL);
	if (!disp_cdev)
		return ERR_PTR(-ENOMEM);
	disp_cdev->thermal_state = 200;
	disp_cdev->bd = bd;

	if (bd->props.max_brightness > BRIGHTNESS_CDEV_MAX)
		disp_cdev->cdev_sf = (bd->props.max_brightness /
						BRIGHTNESS_CDEV_MAX);
	else
		disp_cdev->cdev_sf = 1;

	disp_cdev->cdev = thermal_of_cooling_device_register(dev->of_node,
				(char *)dev_name(&bd->dev), disp_cdev,
				&sde_cdev_ops);
	if (IS_ERR_OR_NULL(disp_cdev->cdev)) {
		pr_err("cooling device register failed\n");
		return (void *)disp_cdev->cdev;
	}
	BLOCKING_INIT_NOTIFIER_HEAD(&disp_cdev->notifier_head);
	blocking_notifier_chain_register(&disp_cdev->notifier_head, n);

	return disp_cdev;
}

void backlight_cdev_unregister(struct sde_cdev *cdev)
{
	if (!cdev)
		return;

	thermal_cooling_device_unregister(cdev->cdev);
}
