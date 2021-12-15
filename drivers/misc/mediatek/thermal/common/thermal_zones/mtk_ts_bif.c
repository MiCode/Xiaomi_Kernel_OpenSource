// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <linux/uidgid.h>
#include <tmp_bts.h>
#include <linux/slab.h>
#include <mt-plat/mtk_battery.h>

/*=============================================================
 *Weak function
 *=============================================================
 */

/*=============================================================
 *Macro
 *=============================================================
 */
static DEFINE_SEMAPHORE(sem_mutex);
static int isTimerCancelled;

#define MTKTS_BIF_TEMP_CRIT 60000	/* 60.000 degree Celsius */

#define mtkts_bif_dprintk(fmt, args...)   \
do {                                    \
	if (mtkts_bif_debug_log) {                \
		pr_debug("[Thermal/TZ/BIF]" fmt, ##args); \
	}                                   \
} while (0)

#define mtkts_bif_printk(fmt, args...) \
pr_debug("[Thermal/TZ/BIF]" fmt, ##args)

/*=============================================================
 *Function prototype
 *=============================================================
 */
static int mtkts_bif_register_thermal(void);
static void mtkts_bif_unregister_thermal(void);
/*=============================================================
 *Local variable
 *=============================================================
 */
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static unsigned int interval = 1;	/* seconds, 0 : no auto polling */
static struct thermal_zone_device *thz_dev;
static int mtkts_bif_debug_log;
static int kernelmode;
static int num_trip;
static int trip_temp[10] = { 120000, 110000, 100000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static char g_bind[10][20] = {"mtktsbif-sysrst", "no-cooler",
				"no-cooler", "no-cooler", "no-cooler",
				"no-cooler", "no-cooler", "no-cooler",
				"no-cooler", "no-cooler"};
/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 *	use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 5000;
static int polling_factor2 = 10000;
static int bif_cur_temp = -127000;
/*=============================================================*/
/*=============================================================
 *Local function
 *=============================================================
 */
static int mtkts_bif_get_temp(struct thermal_zone_device *thermal, int *t)
{
	/* *t = mtkts_bif_get_hw_temp(); */
	int temp;

	if (!pmic_get_bif_battery_temperature(&temp))
		*t = temp * 1000;
	else
		*t = bif_cur_temp; /* Previous temp */

	bif_cur_temp = *t;

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtkts_bif_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int i, table_val = 10;

	for (i = 0; i < 10; i++) {
		if (!strcmp(cdev->type, g_bind[i])) {
			table_val = i;
			mtkts_bif_dprintk("[%s] %s, trip %d\n", __func__,
								cdev->type, i);
			break;
		}
	}

	if (table_val == 10)
		return -EINVAL; /* Not match */

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtkts_bif_dprintk("[%s] error binding cooling dev %s\n",
							__func__, cdev->type);

		return -EINVAL;
	}

	mtkts_bif_dprintk("[%s] binding OK %s, trip %d\n", __func__,
							cdev->type, table_val);
	return 0;
}

static int mtkts_bif_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int i, table_val = 10;

	for (i = 0; i < 10; i++) {
		if (!strcmp(cdev->type, g_bind[i])) {
			table_val = i;
			mtkts_bif_dprintk("[%s] %s, trip %d\n", __func__,
								cdev->type, i);
			break;
		}
	}

	if (table_val == 10)
		return -EINVAL; /* Not match */

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtkts_bif_dprintk("[%s] error unbinding cooling dev %s\n",
							__func__, cdev->type);
		return -EINVAL;
	}

	mtkts_bif_dprintk("[%s] unbinding OK %s, trip %d\n", __func__,
							cdev->type, table_val);
	return 0;
}

static int mtkts_bif_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtkts_bif_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtkts_bif_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtkts_bif_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtkts_bif_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = MTKTS_BIF_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_bif_dev_ops = {
	.bind = mtkts_bif_bind,
	.unbind = mtkts_bif_unbind,
	.get_temp = mtkts_bif_get_temp,
	.get_mode = mtkts_bif_get_mode,
	.set_mode = mtkts_bif_set_mode,
	.get_trip_type = mtkts_bif_get_trip_type,
	.get_trip_temp = mtkts_bif_get_trip_temp,
	.get_crit_temp = mtkts_bif_get_crit_temp,
};

static int mtkts_bif_read(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "%s\n", __func__);
	for (i = 0; i < 10; i++) {
		seq_printf(m, "trip_%d_temp=%d ", i, trip_temp[i]);

		if ((i + 1) % 5 == 0)
			seq_puts(m, "\n");
	}

	for (i = 0; i < 10; i++) {
		seq_printf(m, "g_THERMAL_TRIP_%d=%d ", i, g_THERMAL_TRIP[i]);

		if ((i + 1) % 5 == 0)
			seq_puts(m, "\n");
	}

	for (i = 0; i < 10; i++) {
		seq_printf(m, "cooldev%d=%s ", i, g_bind[i]);

		if ((i + 1) % 5 == 0)
			seq_puts(m, "\n");
	}

	seq_printf(m, "time_ms=%d\n", interval * 1000);
	return 0;
}

static ssize_t mtkts_bif_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, i, j;

	struct tmp_data {
		int trip[10];
		int t_type[10];
		char bind[10][20];
		int time_msec;
		char desc[512];
	};

	struct tmp_data *ptr_tmp_data = kmalloc(
					sizeof(*ptr_tmp_data), GFP_KERNEL);

	if (ptr_tmp_data == NULL)
		return -ENOMEM;


	len = (count < (sizeof(ptr_tmp_data->desc) - 1)) ?
				count : (sizeof(ptr_tmp_data->desc) - 1);

	if (copy_from_user(ptr_tmp_data->desc, buffer, len)) {
		kfree(ptr_tmp_data);
		return 0;
	}

	ptr_tmp_data->desc[len] = '\0';

	if (sscanf
	    (ptr_tmp_data->desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_tmp_data->trip[0], &ptr_tmp_data->t_type[0],
		ptr_tmp_data->bind[0],
		&ptr_tmp_data->trip[1], &ptr_tmp_data->t_type[1],
		ptr_tmp_data->bind[1],
		&ptr_tmp_data->trip[2], &ptr_tmp_data->t_type[2],
		ptr_tmp_data->bind[2],
		&ptr_tmp_data->trip[3], &ptr_tmp_data->t_type[3],
		ptr_tmp_data->bind[3],
		&ptr_tmp_data->trip[4], &ptr_tmp_data->t_type[4],
		ptr_tmp_data->bind[4],
		&ptr_tmp_data->trip[5], &ptr_tmp_data->t_type[5],
		ptr_tmp_data->bind[5],
		&ptr_tmp_data->trip[6], &ptr_tmp_data->t_type[6],
		ptr_tmp_data->bind[6],
		&ptr_tmp_data->trip[7], &ptr_tmp_data->t_type[7],
		ptr_tmp_data->bind[7],
		&ptr_tmp_data->trip[8], &ptr_tmp_data->t_type[8],
		ptr_tmp_data->bind[8],
		&ptr_tmp_data->trip[9], &ptr_tmp_data->t_type[9],
		ptr_tmp_data->bind[9],
		&ptr_tmp_data->time_msec) == 32) {
		down(&sem_mutex);
		mtkts_bif_dprintk("[%s] mtkts_bif_unregister_thermal\n",
								__func__);
		mtkts_bif_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "mtkts_bif_write",
					"Bad argument");
			#endif
			mtkts_bif_dprintk("[%s] bad argument\n", __func__);
			kfree(ptr_tmp_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_tmp_data->t_type[i];

		for (i = 0; i < 10; i++)
			g_bind[i][0] = '\0';

		for (i = 0; i < 10; i++)
			for (j = 0; j < 20; j++)
				g_bind[i][j] = ptr_tmp_data->bind[i][j];

		mtkts_bif_dprintk(
			"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d\n",
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);

		mtkts_bif_dprintk(
			"g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d\n",
			g_THERMAL_TRIP[4], g_THERMAL_TRIP[5],
			g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);

		mtkts_bif_dprintk("g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d\n",
			g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

		mtkts_bif_dprintk(
			"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s\n",
			g_bind[0], g_bind[1], g_bind[2], g_bind[3], g_bind[4]);

		mtkts_bif_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind[5], g_bind[6], g_bind[7], g_bind[8], g_bind[9]);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_tmp_data->trip[i];

		interval = ptr_tmp_data->time_msec / 1000;

		mtkts_bif_dprintk(
			"trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d\n",
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		mtkts_bif_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d\n",
			trip_temp[4], trip_temp[5], trip_temp[6], trip_temp[7]);

		mtkts_bif_dprintk("trip_8_temp=%d,trip_9_temp=%d,time_ms=%d\n",
			trip_temp[8], trip_temp[9], interval * 1000);

		mtkts_bif_dprintk("[%s] mtkts_bif_register_thermal\n",
								__func__);
		mtkts_bif_register_thermal();
		up(&sem_mutex);
		kfree(ptr_tmp_data);
		return count;
	}

	mtkts_bif_dprintk("[%s] bad argument\n", __func__);
    #ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
					"mtkts_bif_write", "Bad argument");
    #endif
	kfree(ptr_tmp_data);
	return -EINVAL;
}

static void mtkts_bif_cancel_thermal_timer(void)
{
	/* cancel timer */
	/* mtkts_bif_printk("mtkts_bif_cancel_thermal_timer\n"); */

	/* stop thermal framework polling when entering deep idle */
	if (down_trylock(&sem_mutex))
		return;

	if (thz_dev) {
		cancel_delayed_work(&(thz_dev->poll_queue));
		isTimerCancelled = 1;
	}
	up(&sem_mutex);
}

static void mtkts_bif_start_thermal_timer(void)
{
	/* mtkts_bif_printk("mtkts_bif_start_thermal_timer\n"); */

	/* resume thermal framework polling when leaving deep idle */

	if (!isTimerCancelled)
		return;


	if (down_trylock(&sem_mutex))
		return;

	if (thz_dev != NULL && interval != 0) {
		mod_delayed_work(system_freezable_power_efficient_wq,
				&(thz_dev->poll_queue), round_jiffies(
						msecs_to_jiffies(3000)));

		isTimerCancelled = 0;
	}
	up(&sem_mutex);
}


static int mtkts_bif_register_thermal(void)
{
	mtkts_bif_dprintk("[%s]\n", __func__);

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtktsbif", num_trip, NULL,
						&mtkts_bif_dev_ops, 0, 0, 0,
						interval * 1000);

	return 0;
}

static void mtkts_bif_unregister_thermal(void)
{
	mtkts_bif_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtkts_bif_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_bif_read, NULL);
}

static const struct file_operations mtkts_bif_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_bif_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_bif_write,
	.release = single_release,
};

static int __init mtkts_bif_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_dir = NULL;

	mtkts_bif_printk("[%s]\n", __func__);

	mtkts_dir = mtk_thermal_get_proc_drv_therm_dir_entry();

	if (!mtkts_dir) {
		mtkts_bif_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry =
		    proc_create("tzbif", 0664, mtkts_dir, &mtkts_bif_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	mtkts_bif_register_thermal();
	mtkTTimer_register("mtktsbif", mtkts_bif_start_thermal_timer,
						mtkts_bif_cancel_thermal_timer);
	return 0;
}

static void __exit mtkts_bif_exit(void)
{
	mtkts_bif_dprintk("[%s]\n", __func__);
	mtkts_bif_unregister_thermal();
	mtkTTimer_unregister("mtktsbif");
}

module_init(mtkts_bif_init);
module_exit(mtkts_bif_exit);
