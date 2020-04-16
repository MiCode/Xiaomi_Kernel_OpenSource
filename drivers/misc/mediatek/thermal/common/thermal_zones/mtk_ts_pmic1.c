/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <mt-plat/upmu_common.h>
#include <tspmic_settings1.h>
#include <linux/uidgid.h>
#include <linux/slab.h>

/*=============================================================
 *Local variable definition
 *=============================================================
 */
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);
static int isTimerCancelled;

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

static unsigned int interval;	/* seconds, 0 : no auto polling */
static unsigned int trip_temp[10] = { 120000, 110000, 100000, 90000, 80000,
					70000, 65000, 60000, 55000, 50000 };

static unsigned int cl_dev_sysrst_state;
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

/*=============================================================*/

static int mtktspmic_get_temp(struct thermal_zone_device *thermal, int *t)
{
	*t = mtktspmic_get_hw_temp_1();

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtktspmic_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktspmic_info("[%s] error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtktspmic_dprintk("[%s] binding OK, %d\n", __func__, table_val);
	return 0;
}

static int mtktspmic_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspmic_dprintk("[%s] %s\n", __func__, cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktspmic_info(
			"[%s] error unbinding cooling dev\n", __func__);

		return -EINVAL;
	}

	mtktspmic_dprintk("[%s] unbinding OK\n", __func__);
	return 0;
}

static int mtktspmic_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktspmic_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktspmic_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktspmic_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktspmic_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = mtktspmic_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktspmic_dev_ops = {
	.bind = mtktspmic_bind,
	.unbind = mtktspmic_unbind,
	.get_temp = mtktspmic_get_temp,
	.get_mode = mtktspmic_get_mode,
	.set_mode = mtktspmic_set_mode,
	.get_trip_type = mtktspmic_get_trip_type,
	.get_trip_temp = mtktspmic_get_trip_temp,
	.get_crit_temp = mtktspmic_get_crit_temp,
};

static int tspmic1_sysrst_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int tspmic1_sysrst_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int tspmic1_sysrst_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		mtktspmic_info("Power/PMIC_Thermal: reset, reset, reset!!!");
		mtktspmic_info("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		mtktspmic_info("*****************************************");
		mtktspmic_info("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();

	}
	return 0;
}

static struct thermal_cooling_device_ops mtktspmic_cooling_sysrst_ops = {
	.get_max_state = tspmic1_sysrst_get_max_state,
	.get_cur_state = tspmic1_sysrst_get_cur_state,
	.set_cur_state = tspmic1_sysrst_set_cur_state,
};

static int mtktspmic_read(struct seq_file *m, void *v)
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

static int mtktspmic_register_thermal(void);
static void mtktspmic_unregister_thermal(void);

static ssize_t mtktspmic_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	int i;

	struct mtktspmic_data {
		int trip[10];
		int t_type[10];
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
	char desc[512];
	};

	struct mtktspmic_data *ptr_mtktspmic_data;

	ptr_mtktspmic_data = kmalloc(sizeof(*ptr_mtktspmic_data), GFP_KERNEL);

	if (ptr_mtktspmic_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktspmic_data->desc) - 1)) ?
				count : (sizeof(ptr_mtktspmic_data->desc) - 1);

	if (copy_from_user(ptr_mtktspmic_data->desc, buffer, len)) {
		kfree(ptr_mtktspmic_data);
		return 0;
	}

	ptr_mtktspmic_data->desc[len] = '\0';

	if (sscanf(ptr_mtktspmic_data->desc,
		"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktspmic_data->trip[0], &ptr_mtktspmic_data->t_type[0],
		ptr_mtktspmic_data->bind0,
		&ptr_mtktspmic_data->trip[1], &ptr_mtktspmic_data->t_type[1],
		ptr_mtktspmic_data->bind1,
		&ptr_mtktspmic_data->trip[2], &ptr_mtktspmic_data->t_type[2],
		ptr_mtktspmic_data->bind2,
		&ptr_mtktspmic_data->trip[3], &ptr_mtktspmic_data->t_type[3],
		ptr_mtktspmic_data->bind3,
		&ptr_mtktspmic_data->trip[4], &ptr_mtktspmic_data->t_type[4],
		ptr_mtktspmic_data->bind4,
		&ptr_mtktspmic_data->trip[5], &ptr_mtktspmic_data->t_type[5],
		ptr_mtktspmic_data->bind5,
		&ptr_mtktspmic_data->trip[6], &ptr_mtktspmic_data->t_type[6],
		ptr_mtktspmic_data->bind6,
		&ptr_mtktspmic_data->trip[7], &ptr_mtktspmic_data->t_type[7],
		ptr_mtktspmic_data->bind7,
		&ptr_mtktspmic_data->trip[8], &ptr_mtktspmic_data->t_type[8],
		ptr_mtktspmic_data->bind8,
		&ptr_mtktspmic_data->trip[9], &ptr_mtktspmic_data->t_type[9],
		ptr_mtktspmic_data->bind9,
		&ptr_mtktspmic_data->time_msec) == 32) {

		down(&sem_mutex);
		mtktspmic_dprintk(
			"[%s] mtktspmic_unregister_thermal\n", __func__);

		mtktspmic_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "mtktspmic_write",
					"Bad argument");
			#endif
			mtktspmic_dprintk("[%s] bad argument\n", __func__);
			kfree(ptr_mtktspmic_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktspmic_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktspmic_data->bind0[i];
			g_bind1[i] = ptr_mtktspmic_data->bind1[i];
			g_bind2[i] = ptr_mtktspmic_data->bind2[i];
			g_bind3[i] = ptr_mtktspmic_data->bind3[i];
			g_bind4[i] = ptr_mtktspmic_data->bind4[i];
			g_bind5[i] = ptr_mtktspmic_data->bind5[i];
			g_bind6[i] = ptr_mtktspmic_data->bind6[i];
			g_bind7[i] = ptr_mtktspmic_data->bind7[i];
			g_bind8[i] = ptr_mtktspmic_data->bind8[i];
			g_bind9[i] = ptr_mtktspmic_data->bind9[i];
		}

		mtktspmic_dprintk(
			"[%s] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			__func__,
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		mtktspmic_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtktspmic_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtktspmic_dprintk(
			"[%s] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			__func__,
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		mtktspmic_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktspmic_data->trip[i];

		interval = ptr_mtktspmic_data->time_msec / 1000;

		mtktspmic_dprintk(
			"[%s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			__func__,
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		mtktspmic_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6],
			trip_temp[7], trip_temp[8]);

		mtktspmic_dprintk("trip_9_temp=%d,time_ms=%d\n",
						trip_temp[9], interval * 1000);

		mtktspmic_dprintk(
			"[%s] mtktspmic_register_thermal\n", __func__);

		mtktspmic_register_thermal();
		up(&sem_mutex);
		kfree(ptr_mtktspmic_data);
		return count;
	}

	mtktspmic_dprintk("[%s] bad argument\n", __func__);
    #ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
							"mtktspmic_write",
							"Bad argument");
    #endif
	kfree(ptr_mtktspmic_data);
	return -EINVAL;
}

static void mtkts_pmic_cancel_thermal_timer(void)
{
	/* cancel timer */
	/* pr_debug("mtkts_pmic_cancel_thermal_timer\n"); */

	/* stop thermal framework polling when entering deep idle */
	if (down_trylock(&sem_mutex))
		return;

	if (thz_dev) {
		cancel_delayed_work(&(thz_dev->poll_queue));
		isTimerCancelled = 1;
	}

	up(&sem_mutex);
}


static void mtkts_pmic_start_thermal_timer(void)
{
	/* pr_debug("mtkts_pmic_start_thermal_timer\n"); */
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


static int mtktspmic_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register(
						"mtktspmic1-sysrst", NULL,
						&mtktspmic_cooling_sysrst_ops);
	return 0;
}

static int mtktspmic_register_thermal(void)
{
	mtktspmic_dprintk("[%s]\n", __func__);

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktspmic1", num_trip, NULL,
						&mtktspmic_dev_ops, 0, 0, 0,
						interval * 1000);

	return 0;
}

static void mtktspmic_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktspmic_unregister_thermal(void)
{
	mtktspmic_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtktspmic_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktspmic_read, NULL);
}

static const struct file_operations mtktspmic_fops = {
	.owner = THIS_MODULE,
	.open = mtktspmic_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktspmic_write,
	.release = single_release,
};


static int mtktspmic_read_log(struct seq_file *m, void *v)
{

	seq_printf(m, "%s = %d\n", __func__, mtktspmic_debug_log1);


	return 0;
}

static ssize_t mtktspmic_write_log(
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
		mtktspmic_debug_log1 = log_switch;

		return count;
	}
	mtktspmic_info("%s bad argument\n", __func__);
	return -EINVAL;

}


static int mtktspmic_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, mtktspmic_read_log, NULL);
}

static const struct file_operations mtktspmic_log_fops = {
	.owner = THIS_MODULE,
	.open = mtktspmic_open_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktspmic_write_log,
	.release = single_release,
};

static int __init mtktspmic_init(void)
{
	int err = 0;

	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktspmic_dir = NULL;

	mtktspmic_info("[%s]\n", __func__);

	/*
	 *	bit4	RG_VBUF_EN	1: turn on Vbuf.
	 *						0: turn off Vbuf.
	 *	bit2	RG_VBUF_BYP	1: Bypass Vbuf.
	 *						0: turn on Vbuf.
	 *
	 *	RG_VBUF_EN = 1 / RG_VBUF_BYP = 0
	 *
	 *	pmic_data = ts_pmic_read(0x0E9E);
	 *    if((pmic_data>>4&0x1)!=1 || (pmic_data>>2&0x1)!=0)
	 *	mtktspmic_info("[mtktspmic_init]: Warrning !!!"
	 *				"Need to checking this !!!!!\n");
	 */
	mtktspmic_cali_prepare_1();
	mtktspmic_cali_prepare2_1();

	err = mtktspmic_register_cooler();
	if (err)
		return err;
	err = mtktspmic_register_thermal();
	if (err)
		goto err_unreg;

	mtktspmic_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktspmic_dir) {
		mtktspmic_info("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzpmic1", 0664, mtktspmic_dir,
							&mtktspmic_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("tzpmic_log1", 0644, mtktspmic_dir,
							&mtktspmic_log_fops);
	}

	mtkTTimer_register("mtktspmic1", mtkts_pmic_start_thermal_timer,
					mtkts_pmic_cancel_thermal_timer);

	return 0;

err_unreg:
	mtktspmic_unregister_cooler();
	return err;
}

static void __exit mtktspmic_exit(void)
{
	mtktspmic_info("[%s]\n", __func__);
	mtktspmic_unregister_thermal();
	mtktspmic_unregister_cooler();
	mtkTTimer_unregister("mtktspmic");
}
late_initcall(mtktspmic_init);
module_exit(mtktspmic_exit);
