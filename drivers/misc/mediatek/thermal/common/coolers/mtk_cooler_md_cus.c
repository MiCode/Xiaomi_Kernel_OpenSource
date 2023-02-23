/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <mtk_ccci_common.h>
#include <mtk_cooler_mutt_gen97.h>


#define mtk_cooler_md_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/md " fmt, ##args)

static struct thermal_cooling_device *cl_md_dev = { 0 };
static unsigned int g_cl_id;
static unsigned int g_md_level;

static int mtk_cl_md_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = MAX_NUM_INSTANCE_MTK_COOLER_MUTT - 1;
	return 0;
}

	static int mtk_cl_md_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = g_md_level;
	return 0;
}

static int mtk_cl_md_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	unsigned int cmd = MUTT_TMC_COOLER_LV_DISABLE;
	int ret = -1;

	state = state - 1;

	if (state == g_md_level)
		goto end;

	if (state < MAX_NUM_INSTANCE_MTK_COOLER_MUTT)
		cmd = clmutt_level_selection((int)state, MUTT_NR);
	ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
		ID_THROTTLING_CFG, (char *) &cmd, 4);
	if (ret) {
		mtk_cooler_md_dprintk("set lv%ld failed, ret = %d\n",
			state, ret);
	} else {
		g_md_level = state;
		mtk_cooler_md_dprintk("set lv%ld success!\n", state);
	}

end:
	return 0;
}

static int mtk_cl_md_get_available
(struct thermal_cooling_device *cdev, char *available)
{
	*available = MAX_NUM_INSTANCE_MTK_COOLER_MUTT - 1;

	return 0;
}


/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_md_ops = {
	.get_max_state = mtk_cl_md_get_max_state,
	.get_cur_state = mtk_cl_md_get_cur_state,
	.set_cur_state = mtk_cl_md_set_cur_state,
	.get_available = mtk_cl_md_get_available,
};

static int mtk_cooler_md_register_ltf(void)
{
	mtk_cooler_md_dprintk("%s\n", __func__);

	g_cl_id = 0;
	cl_md_dev = mtk_thermal_cooling_device_register
		("mtk-cl-md", (void *)&g_cl_id,
		 &mtk_cl_md_ops);

	return 0;
}

static void mtk_cooler_md_unregister_ltf(void)
{
	mtk_cooler_md_dprintk("%s\n", __func__);
	g_md_level = 255;

	if (cl_md_dev) {
		mtk_thermal_cooling_device_unregister(cl_md_dev);
		cl_md_dev = NULL;
	}
}

static int __init mtk_cooler_md_init(void)
{
	int err = 0;

	err = mtk_cooler_md_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_md_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_md_exit(void)
{
	mtk_cooler_md_unregister_ltf();
}
module_init(mtk_cooler_md_init);
module_exit(mtk_cooler_md_exit);
