// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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
#include <linux/thermal.h>

#include "mi_disp_feature.h"
#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"
#include "mi_disp_print.h"

#define BACKLIGHT_COOLER_NR 1
#define MAX_THERMAL_STATE 200

#define mi_cooler_brightness_clone_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/brightness0-clone " fmt, ##args)

static struct thermal_cooling_device
*cl_backlight_dev[BACKLIGHT_COOLER_NR] = { 0 };

static int mi_cooler_brightness_clone_get_max_state(
		struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	*state = MAX_THERMAL_STATE;
	return 0;
}

static int mi_cooler_brightness_clone_get_cur_state(
		struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	struct mtk_dsi *dsi = NULL;

	dsi = (struct mtk_dsi *)cdev->devdata;

	*state = dsi->mi_cfg.thermal_state;

	return 0;
}

static int mi_cooler_brightness_clone_set_cur_state(
		struct thermal_cooling_device *cdev,
		unsigned long state)
{
#ifndef CONFIG_FACTORY_BUILD
	struct mtk_dsi *dsi = NULL;
	u32 max_brightness_clone = 0;
	u32 thermal_limit_brightness_clone = 0;

	if (state == 0 || state > MAX_THERMAL_STATE) {
		pr_err("%s: invalid state %d\n", __func__, state);
		return -1;
	}

	dsi = (struct mtk_dsi *)cdev->devdata;
	dsi->mi_cfg.thermal_state = state;
	DISP_TIME_INFO("thermal dimming:set thermal_brightness_limit to %d percent\n", state);
	if (dsi->mi_cfg.thermal_dimming_enabled) {
		sysfs_notify(&cdev->device.kobj, NULL, "cur_state");
	} else {
		mi_dsi_display_get_max_brightness_clone(dsi, &max_brightness_clone);
		thermal_limit_brightness_clone = max_brightness_clone * state / MAX_THERMAL_STATE;
		mi_dsi_display_set_thermal_limit_brightness_clone(dsi, thermal_limit_brightness_clone);
	}
#endif
	mi_cooler_brightness_clone_dprintk("%s: %d\n", __func__, state);

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mi_cooler_brightness_clone_ops = {
	.get_max_state = mi_cooler_brightness_clone_get_max_state,
	.get_cur_state = mi_cooler_brightness_clone_get_cur_state,
	.set_cur_state = mi_cooler_brightness_clone_set_cur_state,
};

static int mi_cooler_brightness_clone_register_ltf(struct mtk_dsi *dsi)
{
	mi_cooler_brightness_clone_dprintk("register ltf\n");

	dsi->mi_cfg.thermal_state = MAX_THERMAL_STATE;

	cl_backlight_dev[0] = thermal_cooling_device_register
			("brightness0-clone", (void *)dsi,
			 &mi_cooler_brightness_clone_ops);

	return 0;
}

static void mi_cooler_brightness_clone_unregister_ltf(void)
{
	mi_cooler_brightness_clone_dprintk("unregister ltf\n");

	if (cl_backlight_dev[0]) {
		thermal_cooling_device_unregister
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
