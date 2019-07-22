// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#if 1
#define mtk_cooler_kshutdown_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/kshutdown " fmt, ##args)
#else
#define mtk_cooler_kshutdown_dprintk(fmt, args...)
#endif

#define MAX_NUM_INSTANCE_MTK_COOLER_KSHUTDOWN  8

static struct thermal_cooling_device
*cl_kshutdown_dev[MAX_NUM_INSTANCE_MTK_COOLER_KSHUTDOWN] = { 0 };
static unsigned long cl_kshutdown_state[MAX_NUM_INSTANCE_MTK_COOLER_KSHUTDOWN]
= { 0 };

	static int mtk_cl_kshutdown_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	/* mtk_cooler_kshutdown_dprintk(
	 * "mtk_cl_kshutdown_get_max_state() %s %d\n", cdev->type, *state);
	 */
	return 0;
}

	static int mtk_cl_kshutdown_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
#if 1	/* cannot use this way for now
	 * since devdata is used by mtk_thermal_monitor
	 */
	*state = *((unsigned long *)cdev->devdata);
#else
	*state = cl_kshutdown_state[(int)cdev->type[16]];
#endif
	/* mtk_cooler_kshutdown_dprintk(
	 * "mtk_cl_kshutdown_get_cur_state() %s %d\n",
	 * cdev->type, *state);
	 */
	return 0;
}

	static int mtk_cl_kshutdown_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	/* mtk_cooler_kshutdown_dprintk(
	 * "mtk_cl_kshutdown_set_cur_state() %s %d\n",
	 * cdev->type, state);
	 */
#if 1
	*((unsigned long *)cdev->devdata) = state;
#else
	cl_kshutdown_state[(int)cdev->type[16]] = state;
#endif
	if (state == 1) {
		mtk_cooler_kshutdown_dprintk(
				"%s %s invokes machine_power_off\n", __func__,
				cdev->type);

		machine_power_off();
	}

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_kshutdown_ops = {
	.get_max_state = mtk_cl_kshutdown_get_max_state,
	.get_cur_state = mtk_cl_kshutdown_get_cur_state,
	.set_cur_state = mtk_cl_kshutdown_set_cur_state,
};

static int mtk_cooler_kshutdown_register_ltf(void)
{
	int i;

	mtk_cooler_kshutdown_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_KSHUTDOWN; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-kshutdown%02d", i);
		cl_kshutdown_dev[i] = mtk_thermal_cooling_device_register(
				temp, (void *)&cl_kshutdown_state[i],
				&mtk_cl_kshutdown_ops);
	}

#if 0
	cl_kshutdown_dev = mtk_thermal_cooling_device_register(
			"mtk-cl-shutdown", NULL, &mtk_cl_kshutdown_ops);
#endif

	return 0;
}

static void mtk_cooler_kshutdown_unregister_ltf(void)
{
	int i;

	mtk_cooler_kshutdown_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_KSHUTDOWN; i-- > 0;) {
		if (cl_kshutdown_dev[i]) {
			mtk_thermal_cooling_device_unregister(
					cl_kshutdown_dev[i]);
			cl_kshutdown_dev[i] = NULL;
			cl_kshutdown_state[i] = 0;
		}
	}
#if 0
	if (cl_kshutdown_dev) {
		mtk_thermal_cooling_device_unregister(cl_kshutdown_dev);
		cl_kshutdown_dev = NULL;
	}
#endif
}


static int __init mtk_cooler_kshutdown_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_KSHUTDOWN; i-- > 0;) {
		cl_kshutdown_dev[i] = NULL;
		cl_kshutdown_state[i] = 0;
	}

	/* cl_kshutdown_dev = NULL; */

	mtk_cooler_kshutdown_dprintk("init\n");

	err = mtk_cooler_kshutdown_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

err_unreg:
	mtk_cooler_kshutdown_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_kshutdown_exit(void)
{
	mtk_cooler_kshutdown_dprintk("exit\n");

	mtk_cooler_kshutdown_unregister_ltf();
}
module_init(mtk_cooler_kshutdown_init);
module_exit(mtk_cooler_kshutdown_exit);
