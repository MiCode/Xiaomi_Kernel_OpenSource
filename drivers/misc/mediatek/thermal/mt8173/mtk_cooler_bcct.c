/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <charging.h>
#include "inc/tmp_battery.h"
#include <linux/uidgid.h>

/* ************************************ */
/* Weak functions */
/* ************************************ */
int __attribute__ ((weak))
get_bat_charging_current_level(void)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
	return 500;
}

CHARGER_TYPE __attribute__ ((weak))
mt_get_charger_type(void)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
	return STANDARD_HOST;
}

int __attribute__ ((weak))
set_bat_charging_current_limit(int current_limit)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
	return 0;
}
/* ************************************ */

#define mtk_cooler_bcct_dprintk_always(fmt, args...) \
pr_debug("[thermal/cooler/bcct]" fmt, ##args)

#define mtk_cooler_bcct_dprintk(fmt, args...) \
do { \
	if (1 == cl_bcct_klog_on) \
		pr_debug("[thermal/cooler/bcct]" fmt, ##args); \
} while (0)

#define MAX_NUM_INSTANCE_MTK_COOLER_BCCT  3

#define MTK_CL_BCCT_GET_LIMIT(limit, state) \
{(limit) = (short) (((unsigned long) (state))>>16); }

#define MTK_CL_BCCT_SET_LIMIT(limit, state) \
{(state) = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); }

#define MTK_CL_BCCT_GET_CURR_STATE(curr_state, state) \
{(curr_state) = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_BCCT_SET_CURR_STATE(curr_state, state) \
do { \
	if (0 == (curr_state)) \
		state &= ~0x1; \
	else \
		state |= 0x1; \
} while (0)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static int cl_bcct_klog_on;
static struct thermal_cooling_device *cl_bcct_dev[MAX_NUM_INSTANCE_MTK_COOLER_BCCT] = { 0 };
static unsigned long cl_bcct_state[MAX_NUM_INSTANCE_MTK_COOLER_BCCT] = { 0 };

static int cl_bcct_cur_limit = 65535;

static void mtk_cl_bcct_set_bcct_limit(void)
{
	/* TODO: optimize */
	int i = 0;
	int min_limit = 65535;

	for (; i < MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i++) {
		unsigned long curr_state;

		MTK_CL_BCCT_GET_CURR_STATE(curr_state, cl_bcct_state[i]);
		if (1 == curr_state) {

			int limit;

			MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[i]);
			if ((min_limit > limit) && (limit > 0))
				min_limit = limit;
		}
	}

	if (min_limit != cl_bcct_cur_limit) {
		cl_bcct_cur_limit = min_limit;

		if (65535 <= cl_bcct_cur_limit) {
			set_bat_charging_current_limit(-1);
			mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() limit=-1\n");
		} else {
			/*
			   CHARGER_UNKNOWN = 0,
			   STANDARD_HOST,      // USB : 450mA
			   CHARGING_HOST,
			   NONSTANDARD_CHARGER,// AC : 450mA~1A
			   STANDARD_CHARGER,    // AC : ~1A
			   APPLE_2_1A_CHARGER, // 2.1A apple charger
			   APPLE_1_0A_CHARGER, // 1A apple charger
			   APPLE_0_5A_CHARGER, // 0.5A apple charger
			   WIRELESS_CHARGER,
			 */
			if (mt_get_charger_type() == STANDARD_HOST) {/* usb charger */
				mtk_cooler_bcct_dprintk_always("Current is USB charger, limit=%d\n",
							       cl_bcct_cur_limit);
				if (cl_bcct_cur_limit >= 500) {/* if usb charger, max current can't > 500mA */
					set_bat_charging_current_limit(499);
				} else {
					set_bat_charging_current_limit(cl_bcct_cur_limit);
				}
			} else {
				set_bat_charging_current_limit(cl_bcct_cur_limit);
			}
			mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() limit=%d\n",
						       cl_bcct_cur_limit);
		}

		mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() real limit=%d\n",
					       get_bat_charging_current_level() / 100);

	}
}

static int mtk_cl_bcct_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_bcct_dprintk("mtk_cl_bcct_get_max_state() %s %lu\n", cdev->type, *state);
	return 0;
}

static int mtk_cl_bcct_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	MTK_CL_BCCT_GET_CURR_STATE(*state, *((unsigned long *)cdev->devdata));
	mtk_cooler_bcct_dprintk("mtk_cl_bcct_get_cur_state() %s %lu\n", cdev->type, *state);
	mtk_cooler_bcct_dprintk("mtk_cl_bcct_get_cur_state() %s limit=%d\n", cdev->type,
				get_bat_charging_current_level() / 100);
	return 0;
}

static int mtk_cl_bcct_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	mtk_cooler_bcct_dprintk("mtk_cl_bcct_set_cur_state() %s %lu\n", cdev->type, state);
	MTK_CL_BCCT_SET_CURR_STATE(state, *((unsigned long *)cdev->devdata));
	mtk_cl_bcct_set_bcct_limit();
	mtk_cooler_bcct_dprintk("mtk_cl_bcct_set_cur_state() %s limit=%d\n", cdev->type,
				get_bat_charging_current_level() / 100);

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_bcct_ops = {
	.get_max_state = mtk_cl_bcct_get_max_state,
	.get_cur_state = mtk_cl_bcct_get_cur_state,
	.set_cur_state = mtk_cl_bcct_set_cur_state,
};

static int mtk_cooler_bcct_register_ltf(void)
{
	int i;

	mtk_cooler_bcct_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-bcct%02d", i);
		/* put bcct state to cooler devdata */
		cl_bcct_dev[i] = mtk_thermal_cooling_device_register(temp, (void *)&cl_bcct_state[i],
								     &mtk_cl_bcct_ops);
	}

	return 0;
}

static void mtk_cooler_bcct_unregister_ltf(void)
{
	int i;

	mtk_cooler_bcct_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0;) {
		if (cl_bcct_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_bcct_dev[i]);
			cl_bcct_dev[i] = NULL;
			cl_bcct_state[i] = 0;
		}
	}
}

#if 0
static int _mtk_cl_bcct_proc_read(char *buf, char **start, off_t off, int count, int *eof,
				  void *data)
{
	int len = 0;
	char *p = buf;

	mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_read] invoked.\n");

    /**
     * The format to print out:
     *  kernel_log <0 or 1>
     *  <mtk-cl-bcct<ID>> <bcc limit>
     *  ..
     */
	if (NULL == data) {
		mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_read] null data\n");
	} else {
		int i = 0;

		p += sprintf(p, "klog %d\n", cl_bcct_klog_on);
		p += sprintf(p, "curr_limit %d\n", cl_bcct_cur_limit);

		for (; i < MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i++) {
			int limit;
			unsigned int curr_state;

			MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[i]);
			MTK_CL_BCCT_GET_CURR_STATE(curr_state, cl_bcct_state[i]);

			p += sprintf(p, "mtk-cl-bcct%02d %d mA, state %d\n", i, limit, curr_state);
		}
	}

	*start = buf + off;

	len = p - buf;
	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
}

static ssize_t _mtk_cl_bcct_proc_write(struct file *file, const char *buffer, unsigned long count,
				       void *data)
{
	int len = 0;
	char desc[128];
	int klog_on, limit0, limit1, limit2;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

    /**
     * sscanf format <klog_on> <mtk-cl-bcct00 limit> <mtk-cl-bcct01 limit> ...
     * <klog_on> can only be 0 or 1
     * <mtk-cl-bcct00 limit> can only be positive integer or -1 to denote no limit
     */

	if (NULL == data) {
		mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] null data\n");
		return -EINVAL;
	}
	/* WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 3 */
#if (3 == MAX_NUM_INSTANCE_MTK_COOLER_BCCT)
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[0]);
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[1]);
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[2]);

	if (1 <= sscanf(desc, "%d %d %d %d", &klog_on, &limit0, &limit1, &limit2)) {
		if (klog_on == 0 || klog_on == 1)
			cl_bcct_klog_on = klog_on;

		if (limit0 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit0, cl_bcct_state[0]);
		if (limit1 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit1, cl_bcct_state[1]);
		if (limit2 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit2, cl_bcct_state[2]);

		return count;
	}
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_BCCT!"
#endif
	mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] bad argument\n");
	return -EINVAL;
}
#endif

static ssize_t _cl_bcct_write(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	/* int ret = 0; */
	char tmp[128] = { 0 };
	int klog_on, limit0, limit1, limit2;


	len = (len < (128 - 1)) ? len : (128 - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

    /**
     * sscanf format <klog_on> <mtk-cl-bcct00 limit> <mtk-cl-bcct01 limit> ...
     * <klog_on> can only be 0 or 1
     * <mtk-cl-bcct00 limit> can only be positive integer or -1 to denote no limit
     */

	if (NULL == data) {
		mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] null data\n");
		return -EINVAL;
	}
	/* WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 3 */
#if (3 == MAX_NUM_INSTANCE_MTK_COOLER_BCCT)
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[0]);
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[1]);
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[2]);

	if (1 <= sscanf(tmp, "%d %d %d %d", &klog_on, &limit0, &limit1, &limit2)) {
		if (klog_on == 0 || klog_on == 1)
			cl_bcct_klog_on = klog_on;

		if (limit0 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit0, cl_bcct_state[0]);
		if (limit1 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit1, cl_bcct_state[1]);
		if (limit2 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit2, cl_bcct_state[2]);

		return len;
	}
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_BCCT!"
#endif
	mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] bad argument\n");
	return -EINVAL;
}

static int _cl_bcct_read(struct seq_file *m, void *v)
{
    /**
     * The format to print out:
     *  kernel_log <0 or 1>
     *  <mtk-cl-bcct<ID>> <bcc limit>
     *  ..
     */

	mtk_cooler_bcct_dprintk("_cl_bcct_read invoked.\n");

	{
		int i = 0;

		seq_printf(m, "klog %d\n", cl_bcct_klog_on);
		seq_printf(m, "curr_limit %d\n", cl_bcct_cur_limit);

		for (; i < MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i++) {
			int limit;
			unsigned int curr_state;

			MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[i]);
			MTK_CL_BCCT_GET_CURR_STATE(curr_state, cl_bcct_state[i]);

			seq_printf(m, "mtk-cl-bcct%02d %d mA, state %d\n", i, limit, curr_state);
		}
	}

	return 0;
}

static int _cl_bcct_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_bcct_read, PDE_DATA(inode));
}

static const struct file_operations _cl_bcct_fops = {
	.owner = THIS_MODULE,
	.open = _cl_bcct_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_bcct_write,
	.release = single_release,
};


static int __init mtk_cooler_bcct_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0;) {
		cl_bcct_dev[i] = NULL;
		cl_bcct_state[i] = 0;
	}

	/* cl_bcct_dev = NULL; */

	mtk_cooler_bcct_dprintk("init\n");

	err = mtk_cooler_bcct_register_ltf();
	if (err)
		goto err_unreg;

	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;
		struct proc_dir_entry *dir_entry = NULL;

		dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
		if (!dir_entry) {
			mtk_cooler_bcct_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
						__func__);
		}

		entry =
		    proc_create("clbcct", S_IRUGO | S_IWUSR | S_IWGRP, dir_entry, &_cl_bcct_fops);
		if (!entry)
			mtk_cooler_bcct_dprintk_always("%s clbcct creation failed\n", __func__);
		else
			proc_set_user(entry, uid, gid);
	}
	return 0;

err_unreg:
	mtk_cooler_bcct_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_bcct_exit(void)
{
	mtk_cooler_bcct_dprintk("exit\n");

	/* remove the proc file */
	remove_proc_entry("driver/mtk-cl-bcct", NULL);

	mtk_cooler_bcct_unregister_ltf();
}
module_init(mtk_cooler_bcct_init);
module_exit(mtk_cooler_bcct_exit);
