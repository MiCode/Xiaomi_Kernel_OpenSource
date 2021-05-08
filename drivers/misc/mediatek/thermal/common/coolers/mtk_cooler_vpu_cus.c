/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <tscpu_settings.h>
#include "vpu_dvfs.h"

#define mtk_cooler_vpu_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/vpu " fmt, ##args)

static struct thermal_cooling_device
*cl_vpu_dev = { 0 };
static unsigned int g_cl_id;
static unsigned int g_vpu_level;

static int mtk_cl_vpu_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtk_cooler_vpu_dprintk("%s\n", __func__);
	*state = VPU_OPP_NUM - 1;
	return 0;
}

	static int mtk_cl_vpu_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtk_cooler_vpu_dprintk("%s\n", __func__);

	*state = g_vpu_level;
	return 0;
}

static int mtk_cl_vpu_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	if (g_vpu_opp_table != NULL) {
		if (state >= 0 && state < VPU_OPP_NUM)
			g_vpu_level = state;
		if (g_vpu_level != 0)
			vpu_thermal_en_throttle_cb(0xff, g_vpu_level);
		else
			vpu_thermal_dis_throttle_cb();
	} else
		mtk_cooler_vpu_dprintk(
			"%s: vpu_power table = NULL\n", __func__);

	return 0;
}

#if 0
static int mtk_cl_vpu_get_available
(struct thermal_cooling_device *cdev, char *available)
{
	int i  = 0, len = 0;

	if (g_vpu_opp_table != NULL) {
		for (i = 0; i < VPU_OPP_NUM; i++)
			len += snprintf(available+len, 256, "%d %d\n",
				i, g_vpu_opp_table[i]);
	} else
		mtk_cooler_vpu_dprintk(
			"%s: vpu_power table = NULL\n", __func__);

	return 0;
}
#endif

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_vpu_ops = {
	.get_max_state = mtk_cl_vpu_get_max_state,
	.get_cur_state = mtk_cl_vpu_get_cur_state,
	.set_cur_state = mtk_cl_vpu_set_cur_state,
	//.get_available = mtk_cl_vpu_get_available,
};

static int mtk_cooler_vpu_register_ltf(void)
{
	mtk_cooler_vpu_dprintk("%s\n", __func__);

	g_cl_id = 0;
	cl_vpu_dev = mtk_thermal_cooling_device_register
		("mtk-cl-vpu", (void *)&g_cl_id,
		 &mtk_cl_vpu_ops);

	return 0;
}

static void mtk_cooler_vpu_unregister_ltf(void)
{
	if (cl_vpu_dev) {
		mtk_thermal_cooling_device_unregister(cl_vpu_dev);
		cl_vpu_dev = NULL;
	}
}

static int __init mtk_cooler_vpu_init(void)
{
	int err = 0;

	err = mtk_cooler_vpu_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_vpu_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_vpu_exit(void)
{
	mtk_cooler_vpu_unregister_ltf();
}
module_init(mtk_cooler_vpu_init);
module_exit(mtk_cooler_vpu_exit);
