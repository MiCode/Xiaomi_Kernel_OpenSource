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
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <mt-plat/upmu_common.h>
#include <tspmic_settings.h>
#include <linux/slab.h>

/*=============================================================
 *Local variable definition
 *=============================================================
 */
/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 *	use interval*polling_factor1
 * else, use interval*polling_factor2
 */

/*=============================================================*/
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);
static int isTimerCancelled;


static unsigned int interval;	/* seconds, 0 : no auto polling */
static unsigned int trip_temp[10] = { 120000, 110000, 100000, 90000, 80000,
					70000, 65000, 60000, 55000, 50000 };

/*static unsigned int cl_dev_sysrst_state;*/

static struct thermal_zone_device *thz_dev;

static struct thermal_cooling_device *cl_dev_sysrst;
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

static long int mtktstsx_cur_temp;
/*=============================================================*/

static int mtktstsx_get_temp(
struct thermal_zone_device *thermal, unsigned long *t)
{
	*t = mtktstsx_get_hw_temp();
	mtktstsx_cur_temp = *t;
/*	pr_notice("[mtktstsx_cur_temp] Raw=%d\n", mtktstsx_cur_temp);*/


	thermal->polling_delay = interval * 1000;

#if 0
	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;
#endif
	return 0;
}

static int mtktstsx_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktstsx_info("[%s] error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtktstsx_dprintk("[%s] binding OK, %d\n", __func__, table_val);


	return 0;
}

static int mtktstsx_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktstsx_dprintk("[%s] %s\n", __func__, cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktstsx_info(
			"[%s] error unbinding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtktstsx_dprintk("[%s] unbinding OK\n", __func__);


	return 0;
}

static int mtktstsx_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktstsx_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktstsx_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktstsx_get_trip_temp(
struct thermal_zone_device *thermal, int trip, unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktstsx_get_crit_temp(
struct thermal_zone_device *thermal, unsigned long *temperature)
{
	*temperature = mtktstsx_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktstsx_dev_ops = {
	.bind = mtktstsx_bind,
	.unbind = mtktstsx_unbind,
	.get_temp = mtktstsx_get_temp,
	.get_mode = mtktstsx_get_mode,
	.set_mode = mtktstsx_set_mode,
	.get_trip_type = mtktstsx_get_trip_type,
	.get_trip_temp = mtktstsx_get_trip_temp,
	.get_crit_temp = mtktstsx_get_crit_temp,
};

#if 0
static int tstsx_sysrst_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int tstsx_sysrst_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int tstsx_sysrst_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		mtktstsx_info("Power/PMIC_Thermal: reset, reset, reset!!!");
		mtktstsx_info("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		mtktstsx_info("*****************************************");
		mtktstsx_info("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();
		/* arch_reset(0,NULL); */
	}
	return 0;
}

static struct thermal_cooling_device_ops mtktstsx_cooling_sysrst_ops = {
	.get_max_state = tstsx_sysrst_get_max_state,
	.get_cur_state = tstsx_sysrst_get_cur_state,
	.set_cur_state = tstsx_sysrst_set_cur_state,
};
#endif

static int mtktstsx_read(struct seq_file *m, void *v)
{

	seq_printf(m,
		"[ %s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,\n",
		__func__,
		trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

	seq_printf(m,
		"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[4], trip_temp[5], trip_temp[6],
		trip_temp[7], trip_temp[8], trip_temp[9]);

	seq_printf(m,
		"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,\n",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
		g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);

	seq_printf(m,
		"g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,\n",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5],
		g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);

	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
					g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

	seq_printf(m,
		"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

	seq_printf(m,
		"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static int mtktstsx_register_thermal(void);
static void mtktstsx_unregister_thermal(void);

static ssize_t mtktstsx_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	int i;

	struct mtktstsx_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktstsx_data *ptr_mtktstsx_data;

	ptr_mtktstsx_data = kmalloc(sizeof(*ptr_mtktstsx_data), GFP_KERNEL);

	if (ptr_mtktstsx_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktstsx_data->desc) - 1)) ?
				count : (sizeof(ptr_mtktstsx_data->desc) - 1);

	if (copy_from_user(ptr_mtktstsx_data->desc, buffer, len)) {
		kfree(ptr_mtktstsx_data);
		return 0;
	}

	ptr_mtktstsx_data->desc[len] = '\0';

	if (sscanf(ptr_mtktstsx_data->desc,
		"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktstsx_data->trip[0], &ptr_mtktstsx_data->t_type[0],
		ptr_mtktstsx_data->bind0,
		&ptr_mtktstsx_data->trip[1], &ptr_mtktstsx_data->t_type[1],
		ptr_mtktstsx_data->bind1,
		&ptr_mtktstsx_data->trip[2], &ptr_mtktstsx_data->t_type[2],
		ptr_mtktstsx_data->bind2,
		&ptr_mtktstsx_data->trip[3], &ptr_mtktstsx_data->t_type[3],
		ptr_mtktstsx_data->bind3,
		&ptr_mtktstsx_data->trip[4], &ptr_mtktstsx_data->t_type[4],
		ptr_mtktstsx_data->bind4,
		&ptr_mtktstsx_data->trip[5], &ptr_mtktstsx_data->t_type[5],
		ptr_mtktstsx_data->bind5,
		&ptr_mtktstsx_data->trip[6], &ptr_mtktstsx_data->t_type[6],
		ptr_mtktstsx_data->bind6,
		&ptr_mtktstsx_data->trip[7], &ptr_mtktstsx_data->t_type[7],
		ptr_mtktstsx_data->bind7,
		&ptr_mtktstsx_data->trip[8], &ptr_mtktstsx_data->t_type[8],
		ptr_mtktstsx_data->bind8,
		&ptr_mtktstsx_data->trip[9], &ptr_mtktstsx_data->t_type[9],
		ptr_mtktstsx_data->bind9,
		&ptr_mtktstsx_data->time_msec) == 32) {

		down(&sem_mutex);
		mtktstsx_dprintk(
			"[%s] mtktstsx_unregister_thermal\n", __func__);

		mtktstsx_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "mtktstsx_write",
					"Bad argument");
			#endif
			mtktspmic_dprintk("[%s] bad argument\n", __func__);
			kfree(ptr_mtktstsx_data);
			up(&sem_mutex);
			return -EINVAL;
		}


		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktstsx_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktstsx_data->bind0[i];
			g_bind1[i] = ptr_mtktstsx_data->bind1[i];
			g_bind2[i] = ptr_mtktstsx_data->bind2[i];
			g_bind3[i] = ptr_mtktstsx_data->bind3[i];
			g_bind4[i] = ptr_mtktstsx_data->bind4[i];
			g_bind5[i] = ptr_mtktstsx_data->bind5[i];
			g_bind6[i] = ptr_mtktstsx_data->bind6[i];
			g_bind7[i] = ptr_mtktstsx_data->bind7[i];
			g_bind8[i] = ptr_mtktstsx_data->bind8[i];
			g_bind9[i] = ptr_mtktstsx_data->bind9[i];
		}

		mtktstsx_dprintk(
			"[%s] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			__func__,
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		mtktstsx_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtktstsx_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtktstsx_dprintk(
			"[%s] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			__func__,
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		mtktstsx_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktstsx_data->trip[i];

		interval = ptr_mtktstsx_data->time_msec / 1000;

		mtktstsx_dprintk(
			"[%s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			__func__,
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		mtktstsx_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6],
			trip_temp[7], trip_temp[8]);

		mtktstsx_dprintk("trip_9_temp=%d,time_ms=%d\n",
						trip_temp[9], interval * 1000);

		mtktstsx_dprintk(
			"[%s] mtktstsx_register_thermal\n", __func__);

		mtktstsx_register_thermal();
		up(&sem_mutex);
		kfree(ptr_mtktstsx_data);
		return count;
	}

	mtktstsx_dprintk("[%s] bad argument\n", __func__);

	kfree(ptr_mtktstsx_data);
	return -EINVAL;
}

static void mtkts_tsx_cancel_thermal_timer(void)
{
	/* cancel timer */
	/* pr_debug("mtkts_tsx_cancel_thermal_timer\n"); */

	/* stop thermal framework polling when entering deep idle */
	if (down_trylock(&sem_mutex))
		return;

	if (thz_dev) {
		cancel_delayed_work(&(thz_dev->poll_queue));
		isTimerCancelled = 1;
	}

	up(&sem_mutex);
}


static void mtkts_tsx_start_thermal_timer(void)
{
	/* pr_debug("mtkts_tsx_start_thermal_timer\n"); */
	/* resume thermal framework polling when leaving deep idle */
	if (!isTimerCancelled)
		return;



	if (down_trylock(&sem_mutex))
		return;

	if (thz_dev != NULL && interval != 0) {
		mod_delayed_work(system_freezable_power_efficient_wq,
					&(thz_dev->poll_queue),
					round_jiffies(msecs_to_jiffies(1000)));
		isTimerCancelled = 0;
	}
	up(&sem_mutex);
}

/*
 *int mtktstsx_register_cooler(void)
 *{
 *	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktstsx-sysrst",
 *						NULL,
 *						&mtktstsx_cooling_sysrst_ops);
 *	return 0;
 *}
 */

static int mtktstsx_register_thermal(void)
{
	mtktstsx_dprintk("[%s]\n", __func__);

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktstsx", num_trip, NULL,
						&mtktstsx_dev_ops, 0, 0, 0,
						interval * 1000);

	return 0;
}

void mtktstsx_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktstsx_unregister_thermal(void)
{
	mtktstsx_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtktstsx_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktstsx_read, NULL);
}

static const struct file_operations mtktstsx_fops = {
	.owner = THIS_MODULE,
	.open = mtktstsx_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktstsx_write,
	.release = single_release,
};


static int mtktstsx_read_log(struct seq_file *m, void *v)
{

	seq_printf(m, "%s = %d\n", __func__, mtktstsx_debug_log);


	return 0;
}

static ssize_t mtktstsx_write_log(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int log_switch;
	int len = 0;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &log_switch) == 0) {
		mtktstsx_debug_log = log_switch;

		return count;
	}

	mtktstsx_info("%s bad argument\n", __func__);

	return -EINVAL;

}


static int mtktstsx_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, mtktstsx_read_log, NULL);
}

static const struct file_operations mtktstsx_log_fops = {
	.owner = THIS_MODULE,
	.open = mtktstsx_open_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktstsx_write_log,
	.release = single_release,
};

static int __init mtktstsx_init(void)
{
	int err = 0;

	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktstsx_dir = NULL;

	mtktstsx_info("[%s]\n", __func__);


	err = mtktstsx_register_thermal();
	if (err)
		goto err_unreg;

	mtktstsx_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktstsx_dir) {
		mtktstsx_info("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tztsx", 0664, mtktstsx_dir,
							&mtktstsx_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry =
		    proc_create("tztsx_log", 0644, mtktstsx_dir,
							&mtktstsx_log_fops);
	}

	mtkTTimer_register("mtktstsx", mtkts_tsx_start_thermal_timer,
						mtkts_tsx_cancel_thermal_timer);

	return 0;

err_unreg:
	mtktstsx_unregister_cooler();
	return err;
}

static void __exit mtktstsx_exit(void)
{
	mtktstsx_info("[%s]\n", __func__);
	mtktstsx_unregister_thermal();
	mtktstsx_unregister_cooler();
	mtkTTimer_unregister("mtktstsx");
}
module_init(mtktstsx_init);
module_exit(mtktstsx_exit);
