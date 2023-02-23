/*
 * Copyright (C) 2017 MediaTek Inc.
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

#define BACKLIGHT_COOLER_NR 1
#define MAX_BACKLIGHT_BRIGHTNESS 100

#define mtk_cooler_backlight_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/backlight " fmt, ##args)


static struct thermal_cooling_device
*cl_backlight_dev[BACKLIGHT_COOLER_NR] = { 0 };
static unsigned int g_cl_id[BACKLIGHT_COOLER_NR];
static unsigned int g_backlight_level;

static int mtk_cl_backlight_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = MAX_BACKLIGHT_BRIGHTNESS;
	return 0;
}

	static int mtk_cl_backlight_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = g_backlight_level;
	return 0;
}

static int mtk_cl_backlight_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	int enable = (state >= MAX_BACKLIGHT_BRIGHTNESS) ? 0 : 1;
#if !defined(CONFIG_LEDS_MTK_DISP) && \
		!defined(CONFIG_LEDS_MTK_PWM) && \
		!defined(CONFIG_LEDS_MTK_I2C)
        int temp;

#endif
	state = (state > MAX_BACKLIGHT_BRIGHTNESS)
		? MAX_BACKLIGHT_BRIGHTNESS : state;

#if defined(CONFIG_LEDS_MTK_DISP) || \
		defined(CONFIG_LEDS_MTK_PWM) || \
		defined(CONFIG_LEDS_MTK_I2C)
	mt_leds_max_brightness_set("lcd-backlight", state, enable);
#else
	temp = state * 255 / 100;
	mt_leds_max_brightness_set(temp, enable);
#endif
	g_backlight_level = state;
	mtk_cooler_backlight_dprintk("%s: %d\n", __func__, g_backlight_level);

	return 0;
}

static int mtk_cl_backlight_get_available
(struct thermal_cooling_device *cdev, char *available)
{
	snprintf(available, 4, "%d\n", MAX_BACKLIGHT_BRIGHTNESS);
	return 0;
}


/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_backlight_ops = {
	.get_max_state = mtk_cl_backlight_get_max_state,
	.get_cur_state = mtk_cl_backlight_get_cur_state,
	.set_cur_state = mtk_cl_backlight_set_cur_state,
	.get_available = mtk_cl_backlight_get_available,
};

static int mtk_cooler_backlight_register_ltf(void)
{
	int i;

	mtk_cooler_backlight_dprintk("register ltf\n");

	for (i = 0; i < BACKLIGHT_COOLER_NR; i++) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-backlight");
		/* /< Cooler Name: mtk-cl-backlight01 */

		g_cl_id[i] = i;
		cl_backlight_dev[i] = mtk_thermal_cooling_device_register
			(temp, (void *)&g_cl_id[i],
			 &mtk_cl_backlight_ops);
	}

	return 0;
}

static void mtk_cooler_backlight_unregister_ltf(void)
{
	int i;

	mtk_cooler_backlight_dprintk("unregister ltf\n");

	for (i = 0; i < BACKLIGHT_COOLER_NR; i++) {
		if (cl_backlight_dev[i]) {
			mtk_thermal_cooling_device_unregister
				(cl_backlight_dev[i]);
			cl_backlight_dev[i] = NULL;
		}
	}
}

static int __init mtk_cooler_backlight_init(void)
{
	int err = 0;

	mtk_cooler_backlight_dprintk("init\n");
	g_backlight_level = MAX_BACKLIGHT_BRIGHTNESS;

	err = mtk_cooler_backlight_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_backlight_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_backlight_exit(void)
{
	mtk_cooler_backlight_dprintk("exit\n");

	mtk_cooler_backlight_unregister_ltf();
}
module_init(mtk_cooler_backlight_init);
module_exit(mtk_cooler_backlight_exit);
