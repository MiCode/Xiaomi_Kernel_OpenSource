/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
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

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
/*
The thermal policy is meaningless here.
We only use it to make this thermal zone work.
*/
static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 125000, 110000, 100000, 90000, 80000, 70000, 65000, 60000, 55000, 50000 };
static struct thermal_zone_device *thz_dev;

static int mtktsimgsensor_debug_log;

static int kernelmode;

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;

static char g_bind0[20] = "cpu03";
static char g_bind1[20] = "";
static char g_bind2[20] = "";
static char g_bind3[20] = "";
static char g_bind4[20] = "";
static char g_bind5[20] = "";
static char g_bind6[20] = "";
static char g_bind7[20] = "";
static char g_bind8[20] = "";
static char g_bind9[20] = "";

static char g_imgsensor0[20] = "null";
static char g_imgsensor1[20] = "null";
static char g_imgsensor2[20] = "null";
static char g_imgsensor3[20] = "null";

static long g_temp_imgsensor0 = -275000;
static long g_temp_imgsensor1 = -275000;
static long g_temp_imgsensor2 = -275000;
static long g_temp_imgsensor3 = -275000;

#define mtktsimgsensor_TEMP_CRIT 150000	/* 150.000 degree Celsius */

#define mtktsimgsensor_dprintk(fmt, args...) \
do { \
	if (mtktsimgsensor_debug_log) \
		pr_debug("[thermal/img_sensor]" fmt, ##args); \
} while (0)

static int mtktsimgsensor_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = max(g_temp_imgsensor0, max(g_temp_imgsensor1, max(g_temp_imgsensor2, g_temp_imgsensor3)));
	return 0;
}

static int mtktsimgsensor_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
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
		mtktsimgsensor_dprintk("%s error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtktsimgsensor_dprintk("%s binding OK, %d\n", __func__, table_val);
	return 0;
}

static int mtktsimgsensor_unbind(struct thermal_zone_device *thermal,
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
		mtktsimgsensor_dprintk("%s error unbinding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtktsimgsensor_dprintk("%s unbinding OK\n", __func__);
	return 0;
}

static int mtktsimgsensor_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktsimgsensor_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktsimgsensor_get_trip_type(struct thermal_zone_device *thermal, int trip,
				   enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktsimgsensor_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				   unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktsimgsensor_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = mtktsimgsensor_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktsimgsensor_dev_ops = {
	.bind = mtktsimgsensor_bind,
	.unbind = mtktsimgsensor_unbind,
	.get_temp = mtktsimgsensor_get_temp,
	.get_mode = mtktsimgsensor_get_mode,
	.set_mode = mtktsimgsensor_set_mode,
	.get_trip_type = mtktsimgsensor_get_trip_type,
	.get_trip_temp = mtktsimgsensor_get_trip_temp,
	.get_crit_temp = mtktsimgsensor_get_crit_temp,
};

static int mtktsimgsensor_register_thermal(void);
static void mtktsimgsensor_unregister_thermal(void);

static int mtktsimgsensor_register_thermal(void)
{
	mtktsimgsensor_dprintk("%s\n", __func__);

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktsimgsensor", num_trip, NULL,
				&mtktsimgsensor_dev_ops, 0, 0, 0, interval * 1000);

	return 0;
}

static void mtktsimgsensor_unregister_thermal(void)
{
	mtktsimgsensor_dprintk("%s\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtktsimgsensor_read(struct seq_file *m, void *v)
{
	seq_printf(m, "no_of_trips %d interval %dms\n", num_trip, interval * 1000);
	seq_printf(m, "%d, %d, %s\n", trip_temp[0], g_THERMAL_TRIP[0], g_bind0);
	seq_printf(m, "%d, %d, %s\n", trip_temp[1], g_THERMAL_TRIP[1], g_bind1);
	seq_printf(m, "%d, %d, %s\n", trip_temp[2], g_THERMAL_TRIP[2], g_bind2);
	seq_printf(m, "%d, %d, %s\n", trip_temp[3], g_THERMAL_TRIP[3], g_bind3);
	seq_printf(m, "%d, %d, %s\n", trip_temp[4], g_THERMAL_TRIP[4], g_bind4);
	seq_printf(m, "%d, %d, %s\n", trip_temp[5], g_THERMAL_TRIP[5], g_bind5);
	seq_printf(m, "%d, %d, %s\n", trip_temp[6], g_THERMAL_TRIP[6], g_bind6);
	seq_printf(m, "%d, %d, %s\n", trip_temp[7], g_THERMAL_TRIP[7], g_bind7);
	seq_printf(m, "%d, %d, %s\n", trip_temp[8], g_THERMAL_TRIP[8], g_bind8);
	seq_printf(m, "%d, %d, %s\n", trip_temp[9], g_THERMAL_TRIP[9], g_bind9);

	return 0;
}

static ssize_t mtktsimgsensor_write(struct file *file, const char __user *buffer, size_t count,
			       loff_t *data)
{
	int len = 0, i;
	struct mtktsimgsensor_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktsimgsensor_data *ptr_mtktsimgsensor_data = kmalloc(sizeof(*ptr_mtktsimgsensor_data), GFP_KERNEL);

	if (ptr_mtktsimgsensor_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktsimgsensor_data->desc) - 1)) ?
		count : (sizeof(ptr_mtktsimgsensor_data->desc) - 1);
	if (copy_from_user(ptr_mtktsimgsensor_data->desc, buffer, len)) {
		kfree(ptr_mtktsimgsensor_data);
		return 0;
	}

	ptr_mtktsimgsensor_data->desc[len] = '\0';

	if (sscanf
	    (ptr_mtktsimgsensor_data->desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktsimgsensor_data->trip[0], &ptr_mtktsimgsensor_data->t_type[0], ptr_mtktsimgsensor_data->bind0,
		&ptr_mtktsimgsensor_data->trip[1], &ptr_mtktsimgsensor_data->t_type[1], ptr_mtktsimgsensor_data->bind1,
		&ptr_mtktsimgsensor_data->trip[2], &ptr_mtktsimgsensor_data->t_type[2], ptr_mtktsimgsensor_data->bind2,
		&ptr_mtktsimgsensor_data->trip[3], &ptr_mtktsimgsensor_data->t_type[3], ptr_mtktsimgsensor_data->bind3,
		&ptr_mtktsimgsensor_data->trip[4], &ptr_mtktsimgsensor_data->t_type[4], ptr_mtktsimgsensor_data->bind4,
		&ptr_mtktsimgsensor_data->trip[5], &ptr_mtktsimgsensor_data->t_type[5], ptr_mtktsimgsensor_data->bind5,
		&ptr_mtktsimgsensor_data->trip[6], &ptr_mtktsimgsensor_data->t_type[6], ptr_mtktsimgsensor_data->bind6,
		&ptr_mtktsimgsensor_data->trip[7], &ptr_mtktsimgsensor_data->t_type[7], ptr_mtktsimgsensor_data->bind7,
		&ptr_mtktsimgsensor_data->trip[8], &ptr_mtktsimgsensor_data->t_type[8], ptr_mtktsimgsensor_data->bind8,
		&ptr_mtktsimgsensor_data->trip[9], &ptr_mtktsimgsensor_data->t_type[9], ptr_mtktsimgsensor_data->bind9,
		&ptr_mtktsimgsensor_data->time_msec) == 32) {
		mtktsimgsensor_dprintk("%s legal parameters\n", __func__);
		mtktsimgsensor_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "mtktsimgsensor_write",
					"Bad argument");
			mtktsimgsensor_dprintk("mtktsimgsensor_write bad argument\n");
			kfree(ptr_mtktsimgsensor_data);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktsimgsensor_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] = g_bind4[0] = g_bind5[0] =
		    g_bind6[0] = g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktsimgsensor_data->bind0[i];
			g_bind1[i] = ptr_mtktsimgsensor_data->bind1[i];
			g_bind2[i] = ptr_mtktsimgsensor_data->bind2[i];
			g_bind3[i] = ptr_mtktsimgsensor_data->bind3[i];
			g_bind4[i] = ptr_mtktsimgsensor_data->bind4[i];
			g_bind5[i] = ptr_mtktsimgsensor_data->bind5[i];
			g_bind6[i] = ptr_mtktsimgsensor_data->bind6[i];
			g_bind7[i] = ptr_mtktsimgsensor_data->bind7[i];
			g_bind8[i] = ptr_mtktsimgsensor_data->bind8[i];
			g_bind9[i] = ptr_mtktsimgsensor_data->bind9[i];
		}

		mtktsimgsensor_dprintk("%s\n", __func__);
		mtktsimgsensor_dprintk(
			"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);
		mtktsimgsensor_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4], g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);
		mtktsimgsensor_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
		mtktsimgsensor_dprintk(
			"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		mtktsimgsensor_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktsimgsensor_data->trip[i];

		interval = ptr_mtktsimgsensor_data->time_msec / 1000;

		mtktsimgsensor_dprintk(
			"trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);
		mtktsimgsensor_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6], trip_temp[7], trip_temp[8]);
		mtktsimgsensor_dprintk(
			"trip_9_temp=%d,time_ms=%d\n", trip_temp[9], interval * 1000);

		mtktsimgsensor_register_thermal();

		kfree(ptr_mtktsimgsensor_data);
		return count;
	}

	mtktsimgsensor_dprintk("%s bad argument\n", __func__);
	kfree(ptr_mtktsimgsensor_data);
	return -EINVAL;
}

static int mtktsimgsensor_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktsimgsensor_read, NULL);
}

static const struct file_operations mtktsimgsensor_fops = {
	.owner = THIS_MODULE,
	.open = mtktsimgsensor_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktsimgsensor_write,
	.release = single_release,
};

static int imgsensortemp_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%ld,%ld,%ld,%ld\n",
		g_temp_imgsensor0, g_temp_imgsensor1, g_temp_imgsensor2, g_temp_imgsensor3);
	seq_printf(m, "%s %ld\n", g_imgsensor0, g_temp_imgsensor0);
	seq_printf(m, "%s %ld\n", g_imgsensor1, g_temp_imgsensor1);
	seq_printf(m, "%s %ld\n", g_imgsensor2, g_temp_imgsensor2);
	seq_printf(m, "%s %ld\n", g_imgsensor3, g_temp_imgsensor3);

	return 0;
}

static ssize_t imgsensortemp_write(struct file *file, const char __user *buffer, size_t count,
			       loff_t *data)
{
	int len = 0, i, input_count;
	struct mtktsimgsensor_data {
		char imgsensor0[20], imgsensor1[20], imgsensor2[20], imgsensor3[20];
		long t_imgsensor0;
		long t_imgsensor1;
		long t_imgsensor2;
		long t_imgsensor3;
		char desc[256];
	};

	struct mtktsimgsensor_data *ptr_mtktsimgsensor_data = kmalloc(sizeof(*ptr_mtktsimgsensor_data), GFP_KERNEL);

	if (ptr_mtktsimgsensor_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktsimgsensor_data->desc) - 1)) ?
		count : (sizeof(ptr_mtktsimgsensor_data->desc) - 1);
	if (copy_from_user(ptr_mtktsimgsensor_data->desc, buffer, len)) {
		kfree(ptr_mtktsimgsensor_data);
		return 0;
	}

	ptr_mtktsimgsensor_data->desc[len] = '\0';

	input_count = sscanf
	    (ptr_mtktsimgsensor_data->desc,
		"%ld,%ld,%ld,%ld,%19s,%19s,%19s,%19s",
		&ptr_mtktsimgsensor_data->t_imgsensor0, &ptr_mtktsimgsensor_data->t_imgsensor1,
		&ptr_mtktsimgsensor_data->t_imgsensor2, &ptr_mtktsimgsensor_data->t_imgsensor3,
		ptr_mtktsimgsensor_data->imgsensor0, ptr_mtktsimgsensor_data->imgsensor1,
		ptr_mtktsimgsensor_data->imgsensor2, ptr_mtktsimgsensor_data->imgsensor3);
	if (input_count >= 4) {
		mtktsimgsensor_dprintk("%s legal parameters\n", __func__);

		if (-275 <= ptr_mtktsimgsensor_data->t_imgsensor0 &&
			ptr_mtktsimgsensor_data->t_imgsensor0 <= 200)
			g_temp_imgsensor0 = ptr_mtktsimgsensor_data->t_imgsensor0 * 1000;

		if (-275 <= ptr_mtktsimgsensor_data->t_imgsensor1 &&
			ptr_mtktsimgsensor_data->t_imgsensor1 <= 200)
			g_temp_imgsensor1 = ptr_mtktsimgsensor_data->t_imgsensor1 * 1000;

		if (-275 <= ptr_mtktsimgsensor_data->t_imgsensor2 &&
			ptr_mtktsimgsensor_data->t_imgsensor0 <= 200)
			g_temp_imgsensor2 = ptr_mtktsimgsensor_data->t_imgsensor2 * 1000;

		if (-275 <= ptr_mtktsimgsensor_data->t_imgsensor3 &&
			ptr_mtktsimgsensor_data->t_imgsensor0 <= 200)
			g_temp_imgsensor3 = ptr_mtktsimgsensor_data->t_imgsensor3 * 1000;

		if (input_count == 8) {
			g_imgsensor0[0] = g_imgsensor1[0] = g_imgsensor2[0] = g_imgsensor3[0] = '\0';
			g_imgsensor0[19] = g_imgsensor1[19] = g_imgsensor2[19] = g_imgsensor3[19] = '\0';

			for (i = 0; i < 19; i++) {
				g_imgsensor0[i] = ptr_mtktsimgsensor_data->imgsensor0[i];
				g_imgsensor1[i] = ptr_mtktsimgsensor_data->imgsensor1[i];
				g_imgsensor2[i] = ptr_mtktsimgsensor_data->imgsensor2[i];
				g_imgsensor3[i] = ptr_mtktsimgsensor_data->imgsensor3[i];
			}
		}

		kfree(ptr_mtktsimgsensor_data);
		return count;
	}

	mtktsimgsensor_dprintk("%s bad argument\n", __func__);
	kfree(ptr_mtktsimgsensor_data);
	return -EINVAL;
}

static int imgsensortemp_open(struct inode *inode, struct file *file)
{
	return single_open(file, imgsensortemp_read, NULL);
}

static const struct file_operations imgsensortemp_fops = {
	.owner = THIS_MODULE,
	.open = imgsensortemp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = imgsensortemp_write,
	.release = single_release,
};

static int __init mtktsimgsensor_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktsimgsensor_dir = NULL;

	mtktsimgsensor_dprintk("%s start\n", __func__);

	err = mtktsimgsensor_register_thermal();
	if (err)
		goto err_unreg;

	mtktsimgsensor_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktsimgsensor_dir) {
		mtktsimgsensor_dprintk("%s mkdir /proc/driver/thermal failed\n", __func__);
	} else {
		entry =
		    proc_create("tzimgsensor", S_IRUGO | S_IWUSR | S_IWGRP, mtktsimgsensor_dir,
				&mtktsimgsensor_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry =
		    proc_create("imgsensor_temp", S_IRUGO | S_IWUSR | S_IWGRP, mtktsimgsensor_dir,
				&imgsensortemp_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return 0;

err_unreg:

	return err;
}

static void __exit mtktsimgsensor_exit(void)
{
	mtktsimgsensor_dprintk("%s\n", __func__);
	mtktsimgsensor_unregister_thermal();
}

late_initcall(mtktsimgsensor_init);
module_exit(mtktsimgsensor_exit);

