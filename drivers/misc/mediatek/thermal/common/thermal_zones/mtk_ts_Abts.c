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
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mtk_thermal_typedefs.h"
#include "mach/mt_thermal.h"
#include "mt-plat/mtk_thermal_platform.h"
#include <linux/uidgid.h>
#include <linux/slab.h>

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 200000, 110000, 100000, 90000, 80000, 70000, 65000, 60000, 55000, 50000 };
static struct thermal_zone_device *thz_dev;
static unsigned int cl_dev_sysrst_state;
static struct thermal_cooling_device *cl_dev_sysrst;
static int mtkts_Abts_debug_log;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;
static char g_bind0[20] = "tsAbts-sysrst";
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };


#define MTKTS_Abts_TEMP_CRIT 60000	/* 60.000 degree Celsius */ /* Fix me*/

#define mtkts_Abts_dprintk(fmt, args...)   \
do {                                    \
	if (mtkts_Abts_debug_log) {                \
		pr_debug("[Power/Abts_Thermal]" fmt, ##args); \
	}                                   \
} while (0)

/* This thermal zone of Abts means the area(tsAP, sec) above ATM tpcb target */
static int mtkts_Abts_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	static unsigned long tsAP_Area;
	static int down_weight = 1;
	int tpcb_target = mtk_thermal_get_tpcb_target();
	int tsAP = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP);

	if ((tsAP < 0) || (tpcb_target < 0)) {
		*t = 0;
		return 0;
	}

	/* calculate the integral between tsAP(¢J) and t(sec) */
	if (tsAP >= tpcb_target) {
		tsAP_Area += ((tsAP - tpcb_target)/1000);
		if (tsAP_Area >= 0x1ffff)
			tsAP_Area = 0x1ffff;
	} else {
		if (tsAP_Area <= (((tpcb_target - tsAP)/1000)*down_weight))
			tsAP_Area = 0x0;
		else
			tsAP_Area -= ((tpcb_target - tsAP)/1000);
	}

	*t = tsAP_Area;

	return 0;
}

static int mtkts_Abts_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtkts_Abts_dprintk("[mtkts_Abts_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtkts_Abts_dprintk("[mtkts_Abts_bind] error binding cooling dev\n");
		return -EINVAL;
	}

	mtkts_Abts_dprintk("[mtkts_Abts_bind] binding OK, %d\n", table_val);
	return 0;
}

static int mtkts_Abts_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtkts_Abts_dprintk("[mtkts_Abts_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	}

	mtkts_Abts_dprintk("[mtkts_Abts_unbind] unbinding OK\n");
	return 0;
}

static int mtkts_Abts_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtkts_Abts_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtkts_Abts_get_trip_type(struct thermal_zone_device *thermal, int trip,
				   enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtkts_Abts_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				   unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtkts_Abts_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = MTKTS_Abts_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_Abts_dev_ops = {
	.bind = mtkts_Abts_bind,
	.unbind = mtkts_Abts_unbind,
	.get_temp = mtkts_Abts_get_temp,
	.get_mode = mtkts_Abts_get_mode,
	.set_mode = mtkts_Abts_set_mode,
	.get_trip_type = mtkts_Abts_get_trip_type,
	.get_trip_temp = mtkts_Abts_get_trip_temp,
	.get_crit_temp = mtkts_Abts_get_crit_temp,
};



static int mtkts_Abts_read(struct seq_file *m, void *v)
{

	seq_printf(m, "[mtkts_Abts_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d\n",
		trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3], trip_temp[4]);
	seq_printf(m, "trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[5], trip_temp[6], trip_temp[7], trip_temp[8], trip_temp[9]);
	seq_printf(m, "g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,\n",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);
	seq_printf(m, "g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,\n",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5], g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);
	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n", g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
	seq_printf(m, "cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
	seq_printf(m, "cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static int mtkts_Abts_register_thermal(void);
static void mtkts_Abts_unregister_thermal(void);

static ssize_t mtkts_Abts_write(struct file *file, const char __user *buffer, size_t count,
			       loff_t *data)
{
	int len = 0, i;

	struct mtktsAbts_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktsAbts_data *ptr_mtktsAbts_data = kmalloc(sizeof(*ptr_mtktsAbts_data), GFP_KERNEL);

	if (ptr_mtktsAbts_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktsAbts_data->desc) - 1)) ? count : (sizeof(ptr_mtktsAbts_data->desc) - 1);
	if (copy_from_user(ptr_mtktsAbts_data->desc, buffer, len)) {
		kfree(ptr_mtktsAbts_data);
		return 0;
	}

	ptr_mtktsAbts_data->desc[len] = '\0';

	if (sscanf
	    (ptr_mtktsAbts_data->desc,
	     "%d %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d",
		&num_trip,
		&ptr_mtktsAbts_data->trip[0], &ptr_mtktsAbts_data->t_type[0], ptr_mtktsAbts_data->bind0,
		&ptr_mtktsAbts_data->trip[1], &ptr_mtktsAbts_data->t_type[1], ptr_mtktsAbts_data->bind1,
		&ptr_mtktsAbts_data->trip[2], &ptr_mtktsAbts_data->t_type[2], ptr_mtktsAbts_data->bind2,
		&ptr_mtktsAbts_data->trip[3], &ptr_mtktsAbts_data->t_type[3], ptr_mtktsAbts_data->bind3,
		&ptr_mtktsAbts_data->trip[4], &ptr_mtktsAbts_data->t_type[4], ptr_mtktsAbts_data->bind4,
		&ptr_mtktsAbts_data->trip[5], &ptr_mtktsAbts_data->t_type[5], ptr_mtktsAbts_data->bind5,
		&ptr_mtktsAbts_data->trip[6], &ptr_mtktsAbts_data->t_type[6], ptr_mtktsAbts_data->bind6,
		&ptr_mtktsAbts_data->trip[7], &ptr_mtktsAbts_data->t_type[7], ptr_mtktsAbts_data->bind7,
		&ptr_mtktsAbts_data->trip[8], &ptr_mtktsAbts_data->t_type[8], ptr_mtktsAbts_data->bind8,
		&ptr_mtktsAbts_data->trip[9], &ptr_mtktsAbts_data->t_type[9], ptr_mtktsAbts_data->bind9,
		&ptr_mtktsAbts_data->time_msec) == 32) {
		mtkts_Abts_dprintk("[mtkts_Abts_write] mtkts_Abts_unregister_thermal\n");
		mtkts_Abts_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "mtkts_Abts_write",
					"Bad argument");
			mtkts_Abts_dprintk("[mtkts_Abts_write] bad argument\n");
			kfree(ptr_mtktsAbts_data);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktsAbts_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] = g_bind4[0] = g_bind5[0] =
		    g_bind6[0] = g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktsAbts_data->bind0[i];
			g_bind1[i] = ptr_mtktsAbts_data->bind1[i];
			g_bind2[i] = ptr_mtktsAbts_data->bind2[i];
			g_bind3[i] = ptr_mtktsAbts_data->bind3[i];
			g_bind4[i] = ptr_mtktsAbts_data->bind4[i];
			g_bind5[i] = ptr_mtktsAbts_data->bind5[i];
			g_bind6[i] = ptr_mtktsAbts_data->bind6[i];
			g_bind7[i] = ptr_mtktsAbts_data->bind7[i];
			g_bind8[i] = ptr_mtktsAbts_data->bind8[i];
			g_bind9[i] = ptr_mtktsAbts_data->bind9[i];
		}

		mtkts_Abts_dprintk("[mtkts_Abts_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);
		mtkts_Abts_dprintk("g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4], g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);
		mtkts_Abts_dprintk("g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

		mtkts_Abts_dprintk("[mtkts_Abts_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		mtkts_Abts_dprintk("cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktsAbts_data->trip[i];

		interval = ptr_mtktsAbts_data->time_msec / 1000;

		mtkts_Abts_dprintk("[mtkts_Abts_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);
		mtkts_Abts_dprintk("trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6], trip_temp[7], trip_temp[8]);
		mtkts_Abts_dprintk("trip_9_temp=%d,time_ms=%d\n",
			trip_temp[9], interval * 1000);

		mtkts_Abts_dprintk("[mtkts_Abts_write] mtkts_Abts_register_thermal\n");

		mtkts_Abts_register_thermal();
		kfree(ptr_mtktsAbts_data);
		/* AP_write_flag=1; */
		return count;
	}

	mtkts_Abts_dprintk("[mtkts_Abts_write] bad argument\n");
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "mtkts_Abts_write",
			"Bad argument");
	kfree(ptr_mtktsAbts_data);
	return -EINVAL;
}

static int mtkts_Abts_register_thermal(void)
{
	mtkts_Abts_dprintk("[mtkts_Abts_register_thermal]\n");

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtktsAbts", num_trip, NULL,
						   &mtkts_Abts_dev_ops, 0, 0, 0, interval * 1000);

	return 0;
}

static void mtkts_Abts_unregister_thermal(void)
{
	mtkts_Abts_dprintk("[mtkts_Abts_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtkts_Abts_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_Abts_read, NULL);
}

static const struct file_operations mtkts_AP_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_Abts_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_Abts_write,
	.release = single_release,
};

static int tsAbts_sysrst_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtkts_Abts_dprintk("tsAbts_sysrst_get_max_state!!!\n");
	*state = 1;
	return 0;
}

static int tsAbts_sysrst_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	mtkts_Abts_dprintk("tsAbts_sysrst_get_cur_state = %d\n", cl_dev_sysrst_state);
	*state = cl_dev_sysrst_state;
	return 0;
}

static int tsAbts_sysrst_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	mtkts_Abts_dprintk("tsAbts_sysrst_set_cur_state = %d\n", cl_dev_sysrst_state);
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_debug("Power/tsAbts_Thermal: reset, reset, reset!!!");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		pr_debug("*****************************************");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

#ifndef CONFIG_ARM64
		BUG();
#else
		*(unsigned int *)0x0 = 0xdead;	/* To trigger data abort to reset the system for thermal protection. */
#endif
	}
	return 0;
}

static struct thermal_cooling_device_ops tsAbts_cooling_sysrst_ops = {
	.get_max_state = tsAbts_sysrst_get_max_state,
	.get_cur_state = tsAbts_sysrst_get_cur_state,
	.set_cur_state = tsAbts_sysrst_set_cur_state,
};

int tsAbts_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("tsAbts-sysrst", NULL,
							    &tsAbts_cooling_sysrst_ops);
	return 0;
}

void tsAbts_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static int __init mtkts_Abts_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_AP_dir = NULL;

	mtkts_Abts_dprintk("[mtkts_Abts_init]\n");

	err = tsAbts_register_cooler();
	if (err)
		return err;

	err = mtkts_Abts_register_thermal();
	if (err)
		goto err_unreg;

	mtkts_AP_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_AP_dir) {
		mtkts_Abts_dprintk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	} else {
		entry =
		    proc_create("tzAbts", S_IRUGO | S_IWUSR | S_IWGRP, mtkts_AP_dir, &mtkts_AP_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return 0;

err_unreg:

	tsAbts_unregister_cooler();

	return err;
}

static void __exit mtkts_Abts_exit(void)
{
	mtkts_Abts_dprintk("[mtkts_Abts_exit]\n");
	mtkts_Abts_unregister_thermal();
}

module_init(mtkts_Abts_init);
module_exit(mtkts_Abts_exit);
