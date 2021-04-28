/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>

#include "mt-plat/mtk_thermal_monitor.h"

#include "mi_disp_feature.h"
#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"

#define BACKLIGHT_COOLER_NR 1

#define mi_cooler_brightness_clone_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/brightness0-clone " fmt, ##args)

static struct thermal_cooling_device
*cl_backlight_dev[BACKLIGHT_COOLER_NR] = { 0 };

static int mi_cooler_brightness_clone_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct mtk_dsi *dsi = NULL;
	u32 max_brightness_clone = 0;

	dsi = (struct mtk_dsi *)cdev->devdata;

	mi_dsi_display_get_max_brightness_clone(dsi, &max_brightness_clone);

	*state = (unsigned long)max_brightness_clone;
	return 0;
}

	static int mi_cooler_brightness_clone_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct mtk_dsi *dsi = NULL;
	u32 thermal_limit_brightness_clone = 0;

	dsi = (struct mtk_dsi *)cdev->devdata;

	mi_dsi_display_get_thermal_limit_brightness_clone(dsi, &thermal_limit_brightness_clone);

	*state = (unsigned long)thermal_limit_brightness_clone;

	return 0;
}

static int mi_cooler_brightness_clone_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct mtk_dsi *dsi = NULL;

	dsi = (struct mtk_dsi *)cdev->devdata;

	mi_dsi_display_set_thermal_limit_brightness_clone(dsi, state);

	mi_cooler_brightness_clone_dprintk("%s: %d\n", __func__, state);

	return 0;
}

static int mi_cooler_brightness_clone_get_available
(struct thermal_cooling_device *cdev, char *available)
{
	struct mtk_dsi *dsi = NULL;
	int max_brightness_clone = 0;

	dsi = (struct mtk_dsi *)cdev->devdata;

	mi_dsi_display_get_max_brightness_clone(dsi, &max_brightness_clone);

	snprintf(available, 5, "%d\n", max_brightness_clone);
	return 0;
}


/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mi_cooler_brightness_clone_ops = {
	.get_max_state = mi_cooler_brightness_clone_get_max_state,
	.get_cur_state = mi_cooler_brightness_clone_get_cur_state,
	.set_cur_state = mi_cooler_brightness_clone_set_cur_state,
	.get_available = mi_cooler_brightness_clone_get_available,
};

static int mi_cooler_brightness_clone_register_ltf(struct mtk_dsi *dsi)
{
	mi_cooler_brightness_clone_dprintk("register ltf\n");

	cl_backlight_dev[0] = mtk_thermal_cooling_device_register
			("brightness0-clone", (void *)dsi,
			 &mi_cooler_brightness_clone_ops);

	return 0;
}

static void mi_cooler_brightness_clone_unregister_ltf(void)
{
	mi_cooler_brightness_clone_dprintk("unregister ltf\n");

	if (cl_backlight_dev[0]) {
		mtk_thermal_cooling_device_unregister
			(cl_backlight_dev[0]);
		cl_backlight_dev[0] = NULL;
	}
}

int mi_cooler_brightness_clone_init(struct mtk_dsi *display)
{
	int err = 0;

	mi_cooler_brightness_clone_dprintk("init\n");

	err = mi_cooler_brightness_clone_register_ltf(display);
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mi_cooler_brightness_clone_unregister_ltf();
	return err;
}

EXPORT_SYMBOL(mi_cooler_brightness_clone_init);

void mi_cooler_brightness_clone_exit(void)
{
	mi_cooler_brightness_clone_dprintk("exit\n");

	mi_cooler_brightness_clone_unregister_ltf();
}
EXPORT_SYMBOL(mi_cooler_brightness_clone_exit);
