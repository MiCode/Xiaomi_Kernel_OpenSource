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

#include "flashlight-core.h"

#define mtk_cooler_flashlight_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/flashlight " fmt, ##args)

#define FLASHLIGHT_COOLER_NR 1

static struct thermal_cooling_device
*cl_flashlight_dev[FLASHLIGHT_COOLER_NR] = { 0 };
static unsigned int g_cl_id[FLASHLIGHT_COOLER_NR];
static unsigned int g_flashlight_level;
static unsigned int g_max_flashlight_level = 0;

static int mtk_cl_flashlight_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = g_max_flashlight_level;
	return 0;
}

	static int mtk_cl_flashlight_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = g_flashlight_level;
	return 0;
}

static int mtk_cl_flashlight_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	if (state <= g_max_flashlight_level) {
		flashlight_set_cooler_level(state);
		g_flashlight_level = state;
		mtk_cooler_flashlight_dprintk("%s: %d\n",
			__func__, g_flashlight_level);
	}

	return 0;
}

static int mtk_cl_flashlight_get_available
(struct thermal_cooling_device *cdev, char *available)
{
	snprintf(available, 3, "%d\n", g_max_flashlight_level);
	return 0;
}


/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_flashlight_ops = {
	.get_max_state = mtk_cl_flashlight_get_max_state,
	.get_cur_state = mtk_cl_flashlight_get_cur_state,
	.set_cur_state = mtk_cl_flashlight_set_cur_state,
	.get_available = mtk_cl_flashlight_get_available,
};

static int mtk_cooler_flashlight_register_ltf(void)
{
	int i;

	mtk_cooler_flashlight_dprintk("register ltf\n");

	for (i = 0; i < FLASHLIGHT_COOLER_NR; i++) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-flashlight");

		g_cl_id[i] = i;
		cl_flashlight_dev[i] = mtk_thermal_cooling_device_register
			(temp, (void *)&g_cl_id[i],
			 &mtk_cl_flashlight_ops);
	}

	return 0;
}

static void mtk_cooler_flashlight_unregister_ltf(void)
{
	int i;

	mtk_cooler_flashlight_dprintk("unregister ltf\n");

	for (i = 0; i < FLASHLIGHT_COOLER_NR; i++) {
		if (cl_flashlight_dev[i]) {
			mtk_thermal_cooling_device_unregister
				(cl_flashlight_dev[i]);
			cl_flashlight_dev[i] = NULL;
		}
	}
}

static int __init mtk_cooler_flashlight_init(void)
{
	int err = 0;

	mtk_cooler_flashlight_dprintk("init\n");

	err = mtk_cooler_flashlight_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_flashlight_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_flashlight_exit(void)
{
	mtk_cooler_flashlight_dprintk("exit\n");

	mtk_cooler_flashlight_unregister_ltf();
}

static int __init mtk_cooler_flashlight_late_init(void)
{
	g_max_flashlight_level = flashlight_get_max_duty();
	g_flashlight_level = g_max_flashlight_level;

	return 0;
}


module_init(mtk_cooler_flashlight_init);
module_exit(mtk_cooler_flashlight_exit);
late_initcall(mtk_cooler_flashlight_late_init);
