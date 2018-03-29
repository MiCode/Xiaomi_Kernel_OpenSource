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
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mtk_thermal_typedefs.h"
#include "mach/mt_thermal.h"
#include <linux/uidgid.h>
#include <linux/slab.h>

#define MTK_ALLTS_SW_FILTER         (1)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static unsigned int interval = 1000;	/* mseconds, 0 : no auto polling */
static int trip_temp[10] = {120000, 110000, 100000, 90000, 80000,
				 70000, 65000, 60000, 55000, 50000};

static struct thermal_zone_device *thz_dev;

static int tsallts_debug_log;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;
static char g_bind0[20] = { 0 };
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

#define TSALLTS_TEMP_CRIT 120000	/* 120.000 degree Celsius */

#define tsallts_dprintk(fmt, args...)   \
do {                                    \
	if (tsallts_debug_log) {                \
		pr_debug("[Power/ALLTS_Thermal]" fmt, ##args);\
}                                   \
} while (0)

#define tsallts_printk(fmt, args...)   \
	pr_debug("[Power/ALLTS_Thermal]" fmt, ##args)

int mtkts_get_ts3_temp(void)
{
	if (thz_dev)
		return thz_dev->temperature;
	else
		return -127000;
}

static int tsallts_get_temp(struct thermal_zone_device *thermal, unsigned long *t)
{
#if MTK_ALLTS_SW_FILTER == 1
	int curr_temp;
	int temp_temp;
	int ret = 0;
	static int last_abb_read_temp;

	curr_temp = get_immediate_ts3_wrap();
	tsallts_dprintk("tsallts_get_temp CPU ts3 =%d\n", curr_temp);

	/* abnormal high temp */
	if ((curr_temp > (trip_temp[0] - 15000)) || (curr_temp < -30000) || (curr_temp > 85000))
		tsallts_printk("ts3 =%d\n", curr_temp);

	temp_temp = curr_temp;
	if (curr_temp != 0) {/* not the first temp read after resume from suspension */
		if ((curr_temp > 150000) || (curr_temp < -20000)) {	/* invalid range */
			tsallts_printk("ts3 temp invalid=%d\n", curr_temp);
			temp_temp = 50000;
			ret = -1;
		} else if (last_abb_read_temp != 0) {
			/* delta 20C, invalid change */
			if ((curr_temp - last_abb_read_temp > 20000) || (last_abb_read_temp - curr_temp > 20000)) {
				tsallts_printk("ts3 temp float hugely temp=%d, lasttemp=%d\n",
					       curr_temp, last_abb_read_temp);
				temp_temp = 50000;
				ret = -1;
			}
		}
	}

	last_abb_read_temp = curr_temp;
	curr_temp = temp_temp;
	*t = (unsigned long)curr_temp;
	return ret;
#else
	int curr_temp;

	curr_temp = get_immediate_ts3_wrap();
	tsallts_dprintk("tsallts_get_temp ts3 =%d\n", curr_temp);

	if ((curr_temp > (trip_temp[0] - 15000)) || (curr_temp < -30000))
		tsallts_printk("ts3 =%d\n", curr_temp);

	*t = curr_temp;

	return 0;
#endif
}

static int tsallts_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	tsallts_dprintk("[tsallts_bind]\n");
	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		tsallts_dprintk("[tsallts_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		tsallts_dprintk("[tsallts_bind_ts3] error binding cooling dev\n");
		return -EINVAL;
	}

	tsallts_dprintk("[tsallts_bind_ts3] binding OK, %d\n", table_val);
	return 0;
}

static int tsallts_unbind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	tsallts_dprintk("[tsallts_unbind_ts3]\n");
	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		tsallts_dprintk("[tsallts_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		tsallts_dprintk("[tsallts_unbind_ts3] error unbinding cooling dev\n");
		return -EINVAL;
	}

	tsallts_dprintk("[tsallts_unbind_ts3] unbinding OK\n");
	return 0;
}

static int tsallts_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int tsallts_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int tsallts_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int tsallts_get_trip_temp(struct thermal_zone_device *thermal, int trip, unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int tsallts_get_crit_temp(struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = TSALLTS_TEMP_CRIT;
	return 0;
}

void mtkts_allts_cancel_ts3_timer(void)
{
	if (thz_dev)
		cancel_delayed_work(&(thz_dev->poll_queue));
}


void mtkts_allts_start_ts3_timer(void)
{
	if (thz_dev != NULL && interval != 0)
		mod_delayed_work(system_freezable_wq, &(thz_dev->poll_queue), round_jiffies(msecs_to_jiffies(1000)));
	/*1000 = 1sec */
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops tsallts_dev_ops = {
	.bind = tsallts_bind,
	.unbind = tsallts_unbind,
	.get_temp = tsallts_get_temp,
	.get_mode = tsallts_get_mode,
	.set_mode = tsallts_set_mode,
	.get_trip_type = tsallts_get_trip_type,
	.get_trip_temp = tsallts_get_trip_temp,
	.get_crit_temp = tsallts_get_crit_temp,
};

static int tsallts_read(struct seq_file *m, void *v)
{
	seq_printf(m, "[ tsallts_read_ts3] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,\n",
		trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);
	seq_printf(m, "trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[4], trip_temp[5], trip_temp[6], trip_temp[7], trip_temp[8], trip_temp[9]);
	seq_printf(m, "g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,\n",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);
	seq_printf(m, "g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,\n",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5], g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);
	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n", g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
	seq_printf(m, "cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
	seq_printf(m, "cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval);

	return 0;
}

static ssize_t tsallts_write(struct file *file, const char __user *buffer, size_t count,
			     loff_t *data)
{
	int len = 0, i, ret = 0;
	struct temp_data {
		int trip[10];
		int t_type[10];
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
	char desc[512];
	};

	struct temp_data *ptr_temp_data = kmalloc(sizeof(*ptr_temp_data), GFP_KERNEL);

	tsallts_printk("[tsallts_write_ts3]\n");

	if (ptr_temp_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_temp_data->desc) - 1)) ? count : (sizeof(ptr_temp_data->desc) - 1);

	if (copy_from_user(ptr_temp_data->desc, buffer, len))
		goto error_write;

	ptr_temp_data->desc[len] = '\0';

	if (sscanf
	    (ptr_temp_data->desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_temp_data->trip[0], &ptr_temp_data->t_type[0], ptr_temp_data->bind0,
		&ptr_temp_data->trip[1], &ptr_temp_data->t_type[1], ptr_temp_data->bind1,
		&ptr_temp_data->trip[2], &ptr_temp_data->t_type[2], ptr_temp_data->bind2,
		&ptr_temp_data->trip[3], &ptr_temp_data->t_type[3], ptr_temp_data->bind3,
		&ptr_temp_data->trip[4], &ptr_temp_data->t_type[4], ptr_temp_data->bind4,
		&ptr_temp_data->trip[5], &ptr_temp_data->t_type[5], ptr_temp_data->bind5,
		&ptr_temp_data->trip[6], &ptr_temp_data->t_type[6], ptr_temp_data->bind6,
		&ptr_temp_data->trip[7], &ptr_temp_data->t_type[7], ptr_temp_data->bind7,
		&ptr_temp_data->trip[8], &ptr_temp_data->t_type[8], ptr_temp_data->bind8,
		&ptr_temp_data->trip[9], &ptr_temp_data->t_type[9], ptr_temp_data->bind9,
		&ptr_temp_data->time_msec) == 32) {
		tsallts_dprintk("[tsallts_write_ts3] tsallts_unregister_thermal\n");
		if (thz_dev) {
			mtk_thermal_zone_device_unregister(thz_dev);
			thz_dev = NULL;
		}

		if (num_trip < 0 || num_trip > 10) {
			aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "tsallts_write3",
					"Bad argument");
			tsallts_dprintk("[tsallts_write3] bad argument\n");
			ret = -EINVAL;
			goto error_write;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_temp_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] = g_bind4[0] = g_bind5[0] =
		    g_bind6[0] = g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_temp_data->bind0[i];
			g_bind1[i] = ptr_temp_data->bind1[i];
			g_bind2[i] = ptr_temp_data->bind2[i];
			g_bind3[i] = ptr_temp_data->bind3[i];
			g_bind4[i] = ptr_temp_data->bind4[i];
			g_bind5[i] = ptr_temp_data->bind5[i];
			g_bind6[i] = ptr_temp_data->bind6[i];
			g_bind7[i] = ptr_temp_data->bind7[i];
			g_bind8[i] = ptr_temp_data->bind8[i];
			g_bind9[i] = ptr_temp_data->bind9[i];
		}

		tsallts_dprintk("[tsallts_write3] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
				g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);
		tsallts_dprintk("g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
				g_THERMAL_TRIP[3], g_THERMAL_TRIP[4], g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);
		tsallts_dprintk("g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
				g_THERMAL_TRIP[7], g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

		tsallts_dprintk("[tsallts_write3] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,",
				g_bind0, g_bind1, g_bind2, g_bind3);
		tsallts_dprintk("cooldev4=%s,cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
				g_bind4, g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_temp_data->trip[i];

		interval = ptr_temp_data->time_msec;

		tsallts_dprintk("[tsallts_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
				trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);
		tsallts_dprintk("trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
				trip_temp[4], trip_temp[5], trip_temp[6], trip_temp[7], trip_temp[8]);
		tsallts_dprintk("trip_9_temp=%d,time_ms=%d\n", trip_temp[9], interval);

		tsallts_dprintk("[tsallts_write3] tsallts_register_thermal\n");

		if (NULL == thz_dev) {
			thz_dev = mtk_thermal_zone_device_register("mtkts3", num_trip, NULL,
								   &tsallts_dev_ops, 0, 0, 0,
								   interval);
		}

		ret = -EINVAL;
		goto error_write;
	}

	tsallts_dprintk("[tsallts_write3] bad argument\n");
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "tsallts_write3",
			"Bad argument");

error_write:
	kfree(ptr_temp_data);

	return ret;
}

static int tsallts_open(struct inode *inode, struct file *file)
{
	return single_open(file, tsallts_read, NULL);
}

static const struct file_operations tsallts_fops = {
	.owner = THIS_MODULE,
	.open = tsallts_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tsallts_write,
	.release = single_release,
};

static int __init tsallts_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *tsallts_dir = NULL;

	tsallts_dprintk("[tsallts_init] ts3\n");

	tsallts_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!tsallts_dir) {
		tsallts_dprintk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	} else {
		entry =
		    proc_create("tzts3", S_IRUGO | S_IWUSR | S_IWGRP, tsallts_dir, &tsallts_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
	return 0;
}

static void tsallts_unregister_thermal(void)
{
	tsallts_dprintk("[tsallts_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static void __exit tsallts_exit(void)
{
	tsallts_dprintk("[tsallts_exit] ts3\n");
	tsallts_unregister_thermal();
}
module_init(tsallts_init);
module_exit(tsallts_exit);
