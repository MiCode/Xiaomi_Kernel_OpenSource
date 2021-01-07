/*
 * Copyright (C) 2019 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
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
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include "mt-plat/mtk_thermal_monitor.h"
#if (CONFIG_MTK_GAUGE_VERSION == 30)
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_charger.h>
#include <mt-plat/mtk_battery.h>
#else
#include <tmp_battery.h>
#include <charging.h>
#endif

#if (CONFIG_MTK_GAUGE_VERSION == 30)
static struct charger_consumer *thm_chr_consumer;
#endif

/*#define CHAR_CURR_INTERVAL 200 interval  200mA */
#define CHAR_CURR_TABLE_INDEX 16

#define mtk_cooler_char_curr_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/char_curr " fmt, ##args)

static struct thermal_cooling_device
*cl_char_curr_dev = { 0 };
static unsigned int g_cl_id;
static unsigned int g_char_curr_level;
static unsigned int g_thm_en_charging;
static unsigned int max_char_curr_index;

/* char_curr_t = charging current table 
*  -1	: unlimit charging current
*  0	: stop charging
*  positive integer: charging current limit (unit: mA)
*/
static int char_curr_t[CHAR_CURR_TABLE_INDEX] =
	{  -1, 2800, 2600, 2400, 2200,
	 2000, 1800, 1600, 1400, 1200,
	 1000,  800,  600,  400,  200,
	 0};

static int _cl_char_curr_read(struct seq_file *m, void *v)
{
	int i = 0;
	mtk_cooler_char_curr_dprintk("%s\n", __func__);
	seq_printf(m, "charger_current state=%d max=%d\n",
		g_char_curr_level, max_char_curr_index);
	seq_printf(m, "[index, current]\n");
	for (i = 0; i < max_char_curr_index; i++) {
		seq_printf(m, "(%4d,  %4d)\n", i, char_curr_t[i]);
	}

	return 0;
}

static int _cl_char_curr_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_char_curr_read, PDE_DATA(inode));
}

static const struct file_operations _cl_char_curr_fops = {
	.owner = THIS_MODULE,
	.open = _cl_char_curr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};




static int mtk_cl_char_curr_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = max_char_curr_index - 1;
	return 0;
}

	static int mtk_cl_char_curr_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = g_char_curr_level;
	return 0;
}

static int mtk_cl_char_curr_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	int chr_input_curr_limit = 0;

#if (CONFIG_MTK_GAUGE_VERSION == 30)
	if (state >= max_char_curr_index || state < 0) {
		mtk_cooler_char_curr_dprintk("%s: wrong state=%ld %d\n",
			__func__, state, max_char_curr_index);
	} else {
		g_char_curr_level = state; 

		if (g_char_curr_level == 0) {
			chr_input_curr_limit = -1;/* unlimit input current*/
		} else {
			chr_input_curr_limit =
				char_curr_t[g_char_curr_level] * 1000;
		}

		if (chr_input_curr_limit != 0) {
			charger_manager_set_input_current_limit(
				thm_chr_consumer, 0, chr_input_curr_limit);
			mtk_cooler_char_curr_dprintk("%s: l=%d curr=%d\n",
				__func__, g_char_curr_level,
				chr_input_curr_limit);

			if (g_thm_en_charging == 0) {
				g_thm_en_charging = 1;
				charger_manager_enable_power_path(
					thm_chr_consumer, 0,
					g_thm_en_charging);
				mtk_cooler_char_curr_dprintk(
					"%s: en_charging = %d\n",
					__func__, g_thm_en_charging);
			}
		} else if (chr_input_curr_limit == 0 && g_thm_en_charging) {
			g_thm_en_charging = 0;
			charger_manager_enable_power_path(thm_chr_consumer, 0,
				g_thm_en_charging);
		}
	}
#endif
	return 0;
}

static int mtk_cl_char_curr_get_available
(struct thermal_cooling_device *cdev, char *available)
{
	int len = 0, i;

	for (i = 0; i < max_char_curr_index; i++) {
		len += snprintf(available + len, 20, "%d %d\n",
			i, char_curr_t[i]);
	}
	mtk_cooler_char_curr_dprintk("%s\n", available);

	return 0;
}

#if (CONFIG_MTK_GAUGE_VERSION == 30)
static int mtkcooler_char_curr_pdrv_probe(struct platform_device *pdev)
{
	mtk_cooler_char_curr_dprintk("%s\n", __func__);
	thm_chr_consumer = charger_manager_get_by_name(&pdev->dev, "charger");

	return 0;
}

static int mtkcooler_char_curr_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_device mtk_cooler_char_curr_device = {
	.name = "mtk-cooler-char_curr",
	.id = -1,
};

static struct platform_driver mtk_cooler_char_curr_driver = {
	.probe = mtkcooler_char_curr_pdrv_probe,
	.remove = mtkcooler_char_curr_pdrv_remove,
	.driver = {
		.name = "mtk-cooler-char_curr",
		.owner  = THIS_MODULE,
	},
};
static int __init mtkcooler_char_curr_late_init(void)
{
	int ret = 0;

	/* register platform device/driver */
	ret = platform_device_register(&mtk_cooler_char_curr_device);
	if (ret) {
		mtk_cooler_char_curr_dprintk(
					"fail to register device @ %s()\n",
					__func__);
		goto fail;
	}

	ret = platform_driver_register(&mtk_cooler_char_curr_driver);
	if (ret) {
		mtk_cooler_char_curr_dprintk(
					"fail to register driver @ %s()\n",
					__func__);
		goto reg_platform_driver_fail;
	}

	return ret;

reg_platform_driver_fail:
	platform_device_unregister(&mtk_cooler_char_curr_device);

fail:
	return ret;
}
#endif

static struct thermal_cooling_device_ops mtk_cl_char_curr_ops = {
	.get_max_state = mtk_cl_char_curr_get_max_state,
	.get_cur_state = mtk_cl_char_curr_get_cur_state,
	.set_cur_state = mtk_cl_char_curr_set_cur_state,
	.get_available = mtk_cl_char_curr_get_available,
};

static int mtk_cooler_char_curr_register_ltf(void)
{
	mtk_cooler_char_curr_dprintk("%s\n", __func__);

	g_cl_id = 0;
	cl_char_curr_dev = mtk_thermal_cooling_device_register
		("mtk-cl-char_curr", (void *)&g_cl_id,
		 &mtk_cl_char_curr_ops);

	return 0;
}

static void mtk_cooler_char_curr_unregister_ltf(void)
{
	mtk_cooler_char_curr_dprintk("%s\n", __func__);

	if (cl_char_curr_dev) {
		mtk_thermal_cooling_device_unregister(cl_char_curr_dev);
		cl_char_curr_dev = NULL;
	}
}

static int __init mtk_cooler_char_curr_init(void)
{
	int err = 0;

	err = mtk_cooler_char_curr_register_ltf();
	if (err)
		goto err_unreg;

	proc_create("thm_char_curr", 0444, NULL, &_cl_char_curr_fops);
	max_char_curr_index = CHAR_CURR_TABLE_INDEX;
	g_thm_en_charging = 1;

	return 0;

err_unreg:
	mtk_cooler_char_curr_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_char_curr_exit(void)
{
	mtk_cooler_char_curr_unregister_ltf();
}
module_init(mtk_cooler_char_curr_init);
module_exit(mtk_cooler_char_curr_exit);
#if (CONFIG_MTK_GAUGE_VERSION == 30)
late_initcall(mtkcooler_char_curr_late_init);
#endif

