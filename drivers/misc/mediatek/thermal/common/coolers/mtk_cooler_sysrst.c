// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#if IS_ENABLED(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <tscpu_settings.h>

/*=============================================================
 *Local variable definition
 *=============================================================
 */
static unsigned int cl_dev_sysrst_state;
static unsigned int cl_dev_sysrst_state_buck;
static unsigned int cl_dev_sysrst_state_tsap;
#if IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)
static unsigned int cl_dev_sysrst_state_tsbif;
#endif
static struct thermal_cooling_device *cl_dev_sysrst;
static struct thermal_cooling_device *cl_dev_sysrst_buck;
static struct thermal_cooling_device *cl_dev_sysrst_tsap;
#if IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)
static struct thermal_cooling_device *cl_dev_sysrst_tsbif;
#endif
/*=============================================================
 */

/*
 * cooling device callback functions (tscpu_cooling_sysrst_ops)
 * 1 : ON and 0 : OFF
 */
static int sysrst_cpu_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("sysrst_cpu_get_max_state\n"); */
	*state = 1;
	return 0;
}

static int sysrst_cpu_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("sysrst_cpu_get_cur_state\n"); */
	*state = cl_dev_sysrst_state;
	return 0;
}

static int sysrst_cpu_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
#if IS_ENABLED(CONFIG_LVTS_DYNAMIC_ENABLE_REBOOT)
	int tpcb = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP);
#endif

	cl_dev_sysrst_state = state;

	if (cl_dev_sysrst_state == 1) {
		tscpu_printk("%s, CPU T=%d, BTS T=%d\n", __func__,
			mtk_thermal_get_temp(MTK_THERMAL_SENSOR_CPU),
			mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP));
		tscpu_printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		tscpu_printk("*****************************************\n");
		tscpu_printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

#if IS_ENABLED(CONFIG_LVTS_DYNAMIC_ENABLE_REBOOT)
		if (tpcb > DYNAMIC_REBOOT_TRIP_TEMP) {
			tscpu_printk("SW reset! tpcb = %d\n", tpcb);
			BUG_ON(1);
		} else {
			tscpu_printk("Skip SW reset! tpcb = %d\n", tpcb);
		}
#else
		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG_ON(1);
#endif

	}
	return 0;
}

static int sysrst_buck_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("sysrst_buck_get_max_state\n"); */
	*state = 1;
	return 0;
}

static int sysrst_buck_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("sysrst_buck_get_cur_state\n"); */
	*state = cl_dev_sysrst_state_buck;
	return 0;
}

static int sysrst_buck_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state_buck = state;

	if (cl_dev_sysrst_state_buck == 1) {
		tscpu_printk("%s = 1\n", __func__);
		tscpu_printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		tscpu_printk("*****************************************\n");
		tscpu_printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");


		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG_ON(1);

	}
	return 0;
}


static int sysrst_tsap_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("sysrst_tsap_get_max_state\n"); */
	*state = 1;
	return 0;
}

static int sysrst_tsap_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("sysrst_tsap_get_cur_state\n"); */
	*state = cl_dev_sysrst_state_tsap;
	return 0;
}

static int sysrst_tsap_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state_tsap = state;

	if (cl_dev_sysrst_state_tsap == 1) {
		tscpu_printk("sysrst_buck_set_cur_state = 1\n");
		tscpu_printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		tscpu_printk("*****************************************\n");
		tscpu_printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG_ON(1);

	}
	return 0;
}

#if IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)
static int sysrst_tsbif_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("sysrst_tsbif_get_max_state\n"); */
	*state = 1;
	return 0;
}

static int sysrst_tsbif_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("sysrst_tsbif_get_cur_state\n"); */
	*state = cl_dev_sysrst_state_tsbif;
	return 0;
}

static int sysrst_tsbif_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state_tsbif = state;

	if (cl_dev_sysrst_state_tsbif == 1) {
		pr_notice("%s = 1\n", __func__);
		pr_notice("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		pr_notice("*****************************************\n");
		pr_notice("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG_ON(1);

	}
	return 0;
}
#endif

static struct thermal_cooling_device_ops mtktscpu_cooling_sysrst_ops = {
	.get_max_state = sysrst_cpu_get_max_state,
	.get_cur_state = sysrst_cpu_get_cur_state,
	.set_cur_state = sysrst_cpu_set_cur_state,
};

static struct thermal_cooling_device_ops mtktsbuck_cooling_sysrst_ops = {
	.get_max_state = sysrst_buck_get_max_state,
	.get_cur_state = sysrst_buck_get_cur_state,
	.set_cur_state = sysrst_buck_set_cur_state,
};

static struct thermal_cooling_device_ops mtktsap_cooling_sysrst_ops = {
	.get_max_state = sysrst_tsap_get_max_state,
	.get_cur_state = sysrst_tsap_get_cur_state,
	.set_cur_state = sysrst_tsap_set_cur_state,
};

#if IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)
static struct thermal_cooling_device_ops mtktsbif_cooling_sysrst_ops = {
	.get_max_state = sysrst_tsbif_get_max_state,
	.get_cur_state = sysrst_tsbif_get_cur_state,
	.set_cur_state = sysrst_tsbif_set_cur_state,
};
#endif

int mtk_cooler_sysrst_init(void)
{
	tscpu_dprintk("%s: Start\n", __func__);
	cl_dev_sysrst = mtk_thermal_cooling_device_register(
						"mtktscpu-sysrst", NULL,
						&mtktscpu_cooling_sysrst_ops);

	cl_dev_sysrst_buck = mtk_thermal_cooling_device_register(
						"mtktsbuck-sysrst", NULL,
						&mtktsbuck_cooling_sysrst_ops);

	cl_dev_sysrst_tsap = mtk_thermal_cooling_device_register(
						"mtktsAP-sysrst", NULL,
						&mtktsap_cooling_sysrst_ops);

#if IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)
	cl_dev_sysrst_tsbif = mtk_thermal_cooling_device_register(
						"mtktsbif-sysrst", NULL,
						&mtktsbif_cooling_sysrst_ops);
#endif

	tscpu_dprintk("%s: End\n", __func__);
	return 0;
}

void mtk_cooler_sysrst_exit(void)
{
	tscpu_dprintk("%s\n", __func__);
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}

	if (cl_dev_sysrst_buck) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst_buck);
		cl_dev_sysrst_buck = NULL;
	}

	if (cl_dev_sysrst_tsap) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst_tsap);
		cl_dev_sysrst_tsap = NULL;
	}

#if IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)
	if (cl_dev_sysrst_tsbif) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst_tsbif);
		cl_dev_sysrst_tsbif = NULL;
	}
#endif

}
//module_init(mtk_cooler_sysrst_init);
//module_exit(mtk_cooler_sysrst_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");

