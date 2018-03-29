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
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mt_thermal.h"
#include <linux/uidgid.h>
#include <linux/slab.h>
#include <charging.h>

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 125000, 110000, 100000, 90000, 80000, 70000, 65000, 60000, 55000, 50000 };
static unsigned int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;

static struct thermal_cooling_device *cl_dev_sysrst;
static int mtktscharger_debug_log;

static int kernelmode;

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;

static unsigned long prev_temp = 30000;

static char g_bind0[20] = "mtktscharger-sysrst";
static char g_bind1[20] = "";
static char g_bind2[20] = "";
static char g_bind3[20] = "";
static char g_bind4[20] = "";
static char g_bind5[20] = "";
static char g_bind6[20] = "";
static char g_bind7[20] = "";
static char g_bind8[20] = "";
static char g_bind9[20] = "";

#define mtktscharger_TEMP_CRIT 150000	/* 150.000 degree Celsius */

#define mtktscharger_dprintk(fmt, args...)			\
do {								\
	if (mtktscharger_debug_log)					\
		pr_debug("[Power/charger_Thermal]" fmt, ##args);	\
} while (0)

static int mtktscharger_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	/*unsigned char val = 0;*/
	int ret = 0;
	int min_temp = 0, max_temp = 0;

	mtktscharger_dprintk("[mtktscharger_get_temp]\n");
	*t = 0;

	ret = mtk_chr_get_tchr(&min_temp, &max_temp);
	if (ret >= 0) {
		*t = max_temp * 1000;
		prev_temp = *t;
	} else
		*t = prev_temp;

	mtktscharger_dprintk("temp =%lu min=%d max=%d ret=%d\n", *t, min_temp, max_temp, ret);
	return 0;
}

static int mtktscharger_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0))
		table_val = 0;
	else if (!strcmp(cdev->type, g_bind1))
		table_val = 1;
	else if (!strcmp(cdev->type, g_bind2))
		table_val = 2;
	else if (!strcmp(cdev->type, g_bind3))
		table_val = 3;
	else if (!strcmp(cdev->type, g_bind4))
		table_val = 4;
	else if (!strcmp(cdev->type, g_bind5))
		table_val = 5;
	else if (!strcmp(cdev->type, g_bind6))
		table_val = 6;
	else if (!strcmp(cdev->type, g_bind7))
		table_val = 7;
	else if (!strcmp(cdev->type, g_bind8))
		table_val = 8;
	else if (!strcmp(cdev->type, g_bind9))
		table_val = 9;
	else
		return 0;

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktscharger_dprintk("[mtktscharger_bind] error binding cooling dev\n");
		return -EINVAL;
	}

	mtktscharger_dprintk("[mtktscharger_bind] binding OK, %d\n", table_val);
	return 0;
}

static int mtktscharger_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0))
		table_val = 0;
	else if (!strcmp(cdev->type, g_bind1))
		table_val = 1;
	else if (!strcmp(cdev->type, g_bind2))
		table_val = 2;
	else if (!strcmp(cdev->type, g_bind3))
		table_val = 3;
	else if (!strcmp(cdev->type, g_bind4))
		table_val = 4;
	else if (!strcmp(cdev->type, g_bind5))
		table_val = 5;
	else if (!strcmp(cdev->type, g_bind6))
		table_val = 6;
	else if (!strcmp(cdev->type, g_bind7))
		table_val = 7;
	else if (!strcmp(cdev->type, g_bind8))
		table_val = 8;
	else if (!strcmp(cdev->type, g_bind9))
		table_val = 9;
	else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktscharger_dprintk("[mtktscharger_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	}

	mtktscharger_dprintk("[mtktscharger_unbind] unbinding OK\n");
	return 0;
}

static int mtktscharger_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktscharger_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktscharger_get_trip_type(struct thermal_zone_device *thermal, int trip,
				   enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktscharger_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				   unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktscharger_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = mtktscharger_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktscharger_dev_ops = {
	.bind = mtktscharger_bind,
	.unbind = mtktscharger_unbind,
	.get_temp = mtktscharger_get_temp,
	.get_mode = mtktscharger_get_mode,
	.set_mode = mtktscharger_set_mode,
	.get_trip_type = mtktscharger_get_trip_type,
	.get_trip_temp = mtktscharger_get_trip_temp,
	.get_crit_temp = mtktscharger_get_crit_temp,
};

static int mtktscharger_sysrst_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtktscharger_dprintk("mtktscharger_sysrst_get_max_state!!!\n");
	*state = 1;
	return 0;
}

static int mtktscharger_sysrst_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtktscharger_dprintk("mtktscharger_sysrst_get_cur_state = %d\n", cl_dev_sysrst_state);
	*state = cl_dev_sysrst_state;
	return 0;
}

static int mtktscharger_sysrst_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	mtktscharger_dprintk("mtktscharger_sysrst_set_cur_state = %d\n", cl_dev_sysrst_state);
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_debug("Power/charger_Thermal: reset, reset, reset!!!");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		pr_debug("*****************************************");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

#if 0 /* ??? */
#ifndef CONFIG_ARM64
		BUG();
#else
		*(unsigned int *)0x0 = 0xdead;	/* To trigger data abort to reset the system for thermal protection. */
#endif
#endif
	}

	return 0;
}

static struct thermal_cooling_device_ops mtktscharger_cooling_sysrst_ops = {
	.get_max_state = mtktscharger_sysrst_get_max_state,
	.get_cur_state = mtktscharger_sysrst_get_cur_state,
	.set_cur_state = mtktscharger_sysrst_set_cur_state,
};

int mtktscharger_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktscharger-sysrst", NULL,
							    &mtktscharger_cooling_sysrst_ops);
	return 0;
}

static int mtktscharger_read(struct seq_file *m, void *v)
{

	seq_printf(m, "[mtktscharger_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\n",
		trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3], trip_temp[4]);
	seq_printf(m, "trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[5], trip_temp[6], trip_temp[7], trip_temp[8], trip_temp[9]);
	seq_printf(m, "g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d\n",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);
	seq_printf(m, "g_THERMAL_TRIP_4=%d, g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d\n",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5], g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);
	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
		g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
	seq_printf(m, "cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
	seq_printf(m, "cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static int mtktscharger_register_thermal(void);
static void mtktscharger_unregister_thermal(void);

static ssize_t mtktscharger_write(struct file *file, const char __user *buffer, size_t count,
			       loff_t *data)
{
	int len = 0, i;
	struct mtktscharger_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktscharger_data *ptr_mtktscharger_data = kmalloc(sizeof(*ptr_mtktscharger_data), GFP_KERNEL);

	if (ptr_mtktscharger_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktscharger_data->desc) - 1)) ? count : (sizeof(ptr_mtktscharger_data->desc) - 1);
	if (copy_from_user(ptr_mtktscharger_data->desc, buffer, len)) {
		kfree(ptr_mtktscharger_data);
		return 0;
	}

	ptr_mtktscharger_data->desc[len] = '\0';

	if (sscanf
	    (ptr_mtktscharger_data->desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktscharger_data->trip[0], &ptr_mtktscharger_data->t_type[0], ptr_mtktscharger_data->bind0,
		&ptr_mtktscharger_data->trip[1], &ptr_mtktscharger_data->t_type[1], ptr_mtktscharger_data->bind1,
		&ptr_mtktscharger_data->trip[2], &ptr_mtktscharger_data->t_type[2], ptr_mtktscharger_data->bind2,
		&ptr_mtktscharger_data->trip[3], &ptr_mtktscharger_data->t_type[3], ptr_mtktscharger_data->bind3,
		&ptr_mtktscharger_data->trip[4], &ptr_mtktscharger_data->t_type[4], ptr_mtktscharger_data->bind4,
		&ptr_mtktscharger_data->trip[5], &ptr_mtktscharger_data->t_type[5], ptr_mtktscharger_data->bind5,
		&ptr_mtktscharger_data->trip[6], &ptr_mtktscharger_data->t_type[6], ptr_mtktscharger_data->bind6,
		&ptr_mtktscharger_data->trip[7], &ptr_mtktscharger_data->t_type[7], ptr_mtktscharger_data->bind7,
		&ptr_mtktscharger_data->trip[8], &ptr_mtktscharger_data->t_type[8], ptr_mtktscharger_data->bind8,
		&ptr_mtktscharger_data->trip[9], &ptr_mtktscharger_data->t_type[9], ptr_mtktscharger_data->bind9,
		&ptr_mtktscharger_data->time_msec) == 32) {
		mtktscharger_dprintk("[mtktscharger_write] mtktscharger_unregister_thermal\n");
		mtktscharger_unregister_thermal();
		if (num_trip < 0 || num_trip > 10) {
			aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "mtktscharger_write",
					"Bad argument");
			mtktscharger_dprintk("[mtktscharger_write] bad argument\n");
			kfree(ptr_mtktscharger_data);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktscharger_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] = g_bind4[0] = g_bind5[0] =
		    g_bind6[0] = g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktscharger_data->bind0[i];
			g_bind1[i] = ptr_mtktscharger_data->bind1[i];
			g_bind2[i] = ptr_mtktscharger_data->bind2[i];
			g_bind3[i] = ptr_mtktscharger_data->bind3[i];
			g_bind4[i] = ptr_mtktscharger_data->bind4[i];
			g_bind5[i] = ptr_mtktscharger_data->bind5[i];
			g_bind6[i] = ptr_mtktscharger_data->bind6[i];
			g_bind7[i] = ptr_mtktscharger_data->bind7[i];
			g_bind8[i] = ptr_mtktscharger_data->bind8[i];
			g_bind9[i] = ptr_mtktscharger_data->bind9[i];
		}

		mtktscharger_dprintk("[mtktscharger_write]
			g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);
		mtktscharger_dprintk("g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,
			g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4], g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);
		mtktscharger_dprintk("g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
		mtktscharger_dprintk("[mtktscharger_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,
			cooldev3=%s,cooldev4=%s,",
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		mtktscharger_dprintk("cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktscharger_data->trip[i];

		interval = ptr_mtktscharger_data->time_msec / 1000;

		mtktscharger_dprintk("[mtktscharger_write] trip_0_temp=%d,trip_1_temp=%d,
			trip_2_temp=%d,trip_3_temp=%d,",
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);
		mtktscharger_dprintk("trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,
			trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6], trip_temp[7], trip_temp[8]);
		mtktscharger_dprintk("trip_9_temp=%d,time_ms=%d\n", trip_temp[9], interval * 1000);

		mtktscharger_dprintk("[mtktscharger_write] mtktscharger_register_thermal\n");
		mtktscharger_register_thermal();

		kfree(ptr_mtktscharger_data);
		return count;
	}

	mtktscharger_dprintk("[mtktscharger_write] bad argument\n");
	kfree(ptr_mtktscharger_data);
	return -EINVAL;
}

static int mtktscharger_register_thermal(void)
{
	mtktscharger_dprintk("[mtktscharger_register_thermal]\n");

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktscharger", num_trip, NULL, /* name: mtktscharger ??? */
						   &mtktscharger_dev_ops, 0, 0, 0, interval * 1000);

	return 0;
}

void mtktscharger_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktscharger_unregister_thermal(void)
{
	mtktscharger_dprintk("[mtktscharger_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtktscharger_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktscharger_read, NULL);
}

static const struct file_operations mtktscharger_fops = {
	.owner = THIS_MODULE,
	.open = mtktscharger_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktscharger_write,
	.release = single_release,
};

static int __init mtktscharger_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktscharger_dir = NULL;

	mtktscharger_dprintk("mtktscharger_init: Start\n");

	/* return 1 means with 6311, else return 0 */
	/* ??? */
/*
	if (is_da9214_exist() == 0) {
		mtktscharger_dprintk("mtktscharger_init: Buck is not exist\n");
		return err;
	}
*/

	err = mtktscharger_register_cooler();
	if (err)
		return err;

	err = mtktscharger_register_thermal();
	if (err)
		goto err_unreg;

	mtktscharger_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscharger_dir) {
		mtktscharger_dprintk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	} else {
		entry =
		    proc_create("tzcharger", S_IRUGO | S_IWUSR | S_IWGRP, mtktscharger_dir,
				&mtktscharger_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return 0;

err_unreg:

	mtktscharger_unregister_cooler();

	return err;
}

static void __exit mtktscharger_exit(void)
{
	mtktscharger_dprintk("[mtktscharger_exit]\n");
	mtktscharger_unregister_thermal();
	mtktscharger_unregister_cooler();
}
late_initcall(mtktscharger_init);
module_exit(mtktscharger_exit);
