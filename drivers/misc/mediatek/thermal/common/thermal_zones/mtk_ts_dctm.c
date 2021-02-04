/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <linux/uidgid.h>
#include <tmp_bts.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include "mt-plat/mtk_thermal_platform.h"

#ifdef CONFIG_PM
#include <linux/suspend.h>
#endif
/*=============================================================
 *Macro
 *=============================================================
 */
#define MTKTS_DCTM_TEMP_CRIT 85000	/* 85.000 degree Celsius */

#define mtkts_dctm_dprintk(fmt, args...)   \
do {                                    \
	if (mtkts_dctm_debug_log) {                \
		pr_debug("[Thermal/TZ/DCTM]" fmt, ##args); \
	}                                   \
} while (0)

#define mtkts_dctm_printk(fmt, args...) \
pr_debug("[Thermal/TZ/DCTM]" fmt, ##args)

/*=============================================================
 *Function prototype
 *=============================================================
 */
static int mtkts_dctm_register_thermal(void);
static void mtkts_dctm_unregister_thermal(void);
/*=============================================================
 *Local variable for thermal zone
 *=============================================================
 */
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);

static unsigned int interval = 1;	/* seconds, 0 : no auto polling */
static struct thermal_zone_device *thz_dev;
static int mtkts_dctm_debug_log;
static int kernelmode;
static int num_trip = 1;
static int trip_temp[10] = { 120000, 110000, 100000, 90000,
	80000, 70000, 65000, 60000, 55000, 50000 };
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static char g_bind[10][20] = {"mtktsdctm-sysrst", "no-cooler",
"no-cooler", "no-cooler", "no-cooler", "no-cooler", "no-cooler",
"no-cooler", "no-cooler", "no-cooler"};
/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 *	use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 1000;
static int polling_factor2 = 1000;

static int g_resume_done = 1;
/* =============================================================
 * Tskin Model Parameters
 * =============================================================
 */
#define SCALE_UNIT_TINT 1000000
#define SCALE_UNIT_T_TRANSIENT 1000000
#define SCALE_UNIT_RHS 10
#define SCALE_UNIT_CAP 1000
#define NUMNODE 3
#define NUMRHS 2
#define ACNZ 9
#define TSKINNODE 2
#define TPCBNODE 0
#define TAMB 26
#define TAMBCOEF 202
#define TPCBCOEF 2095
#define TPCBINIT 30
static int AcMatrixNzRow[ACNZ] = {0, 0, 0, 1, 1, 1, 2, 2, 2};
static int AcMatrixNzCol[ACNZ] = {0, 1, 2, 0, 1, 2, 0, 1, 2};
static int rhsNode[NUMRHS] = {0, 2};
static long long rhs[NUMNODE] = {0, 0, 0};
static long long tn[NUMNODE] = {0, 0, 0};
static long long tn_1[NUMNODE] = {0, 0, 0};
/* =============================================================
 * Runtime Changeable Variables
 * =============================================================
 */
static int tamb = TAMB;
static int tamb_coef = TAMBCOEF;
static int tpcb_coef = TPCBCOEF;
static int tpcbinit = TPCBINIT;
static char *customized_matrix[] = {"AcMatrixNz", "DcMatrix", "CapMatrix"};
static long long DcMatrix[NUMNODE][NUMRHS] = { {4425917, 3580330}, {4192873,
5986269}, {3580330, 12310129} };
static long long AcMatrixNz[ACNZ] = {3397825, 2473916, 45481, 2473916, 4136358,
76043, 45481, 76043, 156376};
static long long CapMatrix[NUMNODE] = {0, 0, 63135};

/* =============================================================
 * Dynamic RC for Tskin to TTj Model Parameters
 * =============================================================
 */
#define INIT_R1_R2  1543 //unit = 1000
#define INIT_R1C1   5000 //unit = 1000
#define DTSKIN_THRES 1000 // dtskin = 1C
#define DRC_UNIT 1000 // R1/R2, R1C1 unit = 1000
#define DRC_Bound 50000
#define TTSKIN 42000 // Target tskin
#define TTj_Limit 85000
#define AVE_WINDOW 100
#define DRC_AVE_WINDOW 50
static int mtkts_dctm_ttj_on;
static int mtkts_dctm_drc_reset;

/* =============================================================
 * Runtime Changeable Variables
 * =============================================================
 */
static int init_r1_r2 = INIT_R1_R2;
static int init_r1c1 = INIT_R1C1;
static int dtskin_thres = DTSKIN_THRES;
static int drc_bound = DRC_Bound;
static int ttskin = TTSKIN;
static int ave_window = AVE_WINDOW;
static int drc_ave_window = DRC_AVE_WINDOW;
static int ttj_limit = TTj_Limit;

static DEFINE_MUTEX(dctm_mutex);

/* =============================================================
 * Tskin calculation functions
 * =============================================================
 */

/*
 * Calculate Initial (Booting) Tskin by Model
 */
static int tskinInit(int tpcbInit)
{
	int i = 0, j = 0;
	long long tInit = 0;

	/* -127 deg: invalid value */
	if (tpcbInit == -127)
		tpcbInit = tpcbinit;

	mutex_lock(&dctm_mutex);
	rhs[TSKINNODE] = (long long)tamb_coef * tamb; // 10^3
	rhs[TPCBNODE]  = (long long)tpcb_coef * tpcbInit; //10^3

	for (i = 0; i < NUMNODE; i++) {
		tInit = 0;
		for (j = 0; j < NUMRHS; j++)
			tInit += DcMatrix[i][j] *
				div_s64(rhs[rhsNode[j]], SCALE_UNIT_RHS);

		tInit = div_s64(tInit, SCALE_UNIT_TINT);
		tn[i] = tInit;
		tn_1[i] = tInit;
	}
	mutex_unlock(&dctm_mutex);

	mtkts_dctm_printk("%s by input tpcbInit = %d\n", __func__, tpcbInit);

	return tn[TSKINNODE];
}

/*
 * Calculate Transient Value of Tskin
 */
static int tskinTransient(int tpcb)
{
	int i = 0;

	/* -127 deg: invalid value */
	if (tpcb == -127)
		return tn[TSKINNODE];

	mutex_lock(&dctm_mutex);
	for (i = 0; i < NUMNODE; i++) {
		rhs[i] = div_s64(CapMatrix[i] * tn_1[i], SCALE_UNIT_CAP);
		tn[i] = 0;
	}
	rhs[TSKINNODE] += tamb_coef * tamb;
	rhs[TPCBNODE]  += tpcb_coef * tpcb;

	for (i = 0; i < ACNZ; i++)
		tn[AcMatrixNzRow[i]] += AcMatrixNz[i] *
			div_s64(rhs[AcMatrixNzCol[i]], SCALE_UNIT_RHS);

	for (i = 0; i < NUMNODE; i++) {
		tn[i] = div_s64(tn[i], SCALE_UNIT_T_TRANSIENT);
		tn_1[i] = tn[i];
	}
	mutex_unlock(&dctm_mutex);

	return tn[TSKINNODE];
}

int tsdctm_thermal_get_ttj_on(void)
{
	mtkts_dctm_dprintk("%s dctm_ttj_on = %d\n",
				__func__, mtkts_dctm_ttj_on);
	return mtkts_dctm_ttj_on;
}


static void dctmdrc_update_params(void)
{
	int ret = 0;

	thermal_dctm_t.t_drc_par.tamb = tamb;
	thermal_dctm_t.t_drc_par.init_r1_r2 = init_r1_r2;
	thermal_dctm_t.t_drc_par.init_r1c1 = init_r1c1;
	thermal_dctm_t.t_drc_par.dtskin_thres = dtskin_thres;
	thermal_dctm_t.t_drc_par.drc_bound = drc_bound;
	thermal_dctm_t.t_drc_par.ttskin = ttskin;
	thermal_dctm_t.t_drc_par.ave_window = ave_window;
	thermal_dctm_t.t_drc_par.drc_ave_window = drc_ave_window;
	thermal_dctm_t.t_drc_par.ttj_limit = ttj_limit;

	ret = wakeup_ta_algo(TA_DCTM_DRC_CFG);
	mtkts_dctm_printk("%s : ret %d\n", __func__, ret);
}

/* =============================================================
 * Thermal zone functions
 * =============================================================
 */
static int mtkts_dctm_get_temp(struct thermal_zone_device *thermal, int *t)
{
	int temp = 0;
	int tpcb = 0;
	int t_ret = 0;

	if (!g_resume_done) {
		mtkts_dctm_printk("%s error , g_resume_done = %d\n",
						__func__, g_resume_done);
		*t = -127000;
		return 0;
	}

	tpcb = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP)/1000;
	temp = tskinTransient(tpcb);

	mtkts_dctm_dprintk("tpcb:%d tskin:%d\n", tpcb, temp);

	/* temp *= 1000; */

	*t = temp;

	if (mtkts_dctm_ttj_on) {
		if (mtkts_dctm_drc_reset) {
			t_ret = wakeup_ta_algo(TA_DCTM_DRC_RST);
			mtkts_dctm_drc_reset = 0;
		} else
			t_ret = wakeup_ta_algo(TA_DCTM_TTJ);
	}

	if (t_ret < 0)
		pr_notice("%s, wakeup_ta_algo out of memory\n", __func__);

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtkts_dctm_bind(struct thermal_zone_device *thermal,
	struct thermal_cooling_device *cdev)
{
	int i, table_val = 10;

	for (i = 0; i < 10; i++) {
		if (!strcmp(cdev->type, g_bind[i])) {
			table_val = i;
			mtkts_dctm_dprintk("[%s] %s, trip %d\n",
				__func__, cdev->type, i);
			break;
		}
	}

	if (table_val == 10)
		return -EINVAL; /* Not match */

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtkts_dctm_dprintk("[%s] error binding cooling dev %s\n",
			__func__, cdev->type);
		return -EINVAL;
	}

	mtkts_dctm_dprintk("[%s] binding OK %s, trip %d\n",
		__func__, cdev->type, table_val);
	return 0;
}

static int mtkts_dctm_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int i, table_val = 10;

	for (i = 0; i < 10; i++) {
		if (!strcmp(cdev->type, g_bind[i])) {
			table_val = i;
			mtkts_dctm_dprintk("[%s] %s, trip %d\n",
				__func__, cdev->type, i);
			break;
		}
	}

	if (table_val == 10)
		return -EINVAL; /* Not match */

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtkts_dctm_dprintk("[%s] error unbinding cooling dev %s\n",
			__func__, cdev->type);
		return -EINVAL;
	}

	mtkts_dctm_dprintk("[%s] unbinding OK %s, trip %d\n",
		__func__, cdev->type, table_val);
	return 0;
}

static int mtkts_dctm_get_mode(struct thermal_zone_device *thermal,
	enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtkts_dctm_set_mode(struct thermal_zone_device *thermal,
	enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtkts_dctm_get_trip_type(struct thermal_zone_device *thermal,
	int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtkts_dctm_get_trip_temp(struct thermal_zone_device *thermal,
	int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtkts_dctm_get_crit_temp(struct thermal_zone_device *thermal,
	int *temperature)
{
	*temperature = MTKTS_DCTM_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_dctm_dev_ops = {
	.bind = mtkts_dctm_bind,
	.unbind = mtkts_dctm_unbind,
	.get_temp = mtkts_dctm_get_temp,
	.get_mode = mtkts_dctm_get_mode,
	.set_mode = mtkts_dctm_set_mode,
	.get_trip_type = mtkts_dctm_get_trip_type,
	.get_trip_temp = mtkts_dctm_get_trip_temp,
	.get_crit_temp = mtkts_dctm_get_crit_temp,
};

static int mtkts_dctm_read(struct seq_file *m, void *v)
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

static ssize_t mtkts_dctm_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	int len = 0, i, j;

	struct tmp_data {
		int trip[10];
		int t_type[10];
		char bind[10][20];
		int time_msec;
		char desc[512];
	};

	struct tmp_data *ptr_tmp_data = kmalloc(sizeof(*ptr_tmp_data),
		GFP_KERNEL);

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
		&ptr_tmp_data->trip[7], &ptr_tmp_data->t_type[7]
		, ptr_tmp_data->bind[7],
		&ptr_tmp_data->trip[8], &ptr_tmp_data->t_type[8],
		ptr_tmp_data->bind[8],
		&ptr_tmp_data->trip[9], &ptr_tmp_data->t_type[9],
		ptr_tmp_data->bind[9],
		&ptr_tmp_data->time_msec) == 32) {

		down(&sem_mutex);
		mtkts_dctm_dprintk("[%s] mtkts_dctm_unregister_thermal\n",
			__func__);
		mtkts_dctm_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DEFAULT, "mtkts_dctm_write",
				"Bad argument");
			#endif
			mtkts_dctm_printk("[%s] bad argument\n", __func__);
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

		mtkts_dctm_dprintk(
			"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		mtkts_dctm_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtkts_dctm_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtkts_dctm_dprintk(
			"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			g_bind[0], g_bind[1], g_bind[2], g_bind[3], g_bind[4]);

		mtkts_dctm_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind[5], g_bind[6], g_bind[7], g_bind[8], g_bind[9]);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_tmp_data->trip[i];

		interval = ptr_tmp_data->time_msec / 1000;

		mtkts_dctm_dprintk(
			"trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		mtkts_dctm_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6],
			trip_temp[7], trip_temp[8]);

		mtkts_dctm_dprintk("trip_9_temp=%d,time_ms=%d\n",
			trip_temp[9], interval * 1000);

		mtkts_dctm_dprintk("[%s] mtkts_dctm_register_thermal\n",
			__func__);

		mtkts_dctm_register_thermal();
		up(&sem_mutex);

		kfree(ptr_tmp_data);
		return count;
	}

	mtkts_dctm_dprintk("[%s] bad argument\n", __func__);
    #ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__,
		DB_OPT_DEFAULT, "mtkts_dctm_write",
		"Bad argument");
    #endif
	kfree(ptr_tmp_data);
	return -EINVAL;
}
#if 0
static void mtkts_dctm_cancel_thermal_timer(void)
{
	/* cancel timer */
	/* mtkts_dctm_printk("mtkts_dctm_cancel_thermal_timer\n"); */

	/* stop thermal framework polling when entering deep idle */

	if (thz_dev)
		cancel_delayed_work(&(thz_dev->poll_queue));
}

static void mtkts_dctm_start_thermal_timer(void)
{
	/* mtkts_dctm_printk("mtkts_dctm_start_thermal_timer\n"); */

	/* resume thermal framework polling when leaving deep idle */

	if (thz_dev != NULL && interval != 0)
		mod_delayed_work(system_freezable_power_efficient_wq,
			&(thz_dev->poll_queue),
			round_jiffies(msecs_to_jiffies(3000)));
}
#endif
#ifdef CONFIG_PM
static int dctm_pm_event(
		struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&dctm_mutex);
		g_resume_done = 0;
		mutex_unlock(&dctm_mutex);
		break;
	case PM_POST_SUSPEND:
		mtkts_dctm_drc_reset = 1;
		tskinInit(mtk_thermal_get_temp(
					MTK_THERMAL_SENSOR_AP)/1000);
		mutex_lock(&dctm_mutex);
		g_resume_done = 1;
		mutex_unlock(&dctm_mutex);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block dctm_pm_notifier_func = {
	.notifier_call = dctm_pm_event,
	.priority = 0,
};
#endif

static int mtkts_dctm_register_thermal(void)
{
	mtkts_dctm_dprintk("[%s]\n", __func__);

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtktsdctm", num_trip, NULL,
						   &mtkts_dctm_dev_ops, 0, 0, 0,
						   interval * 1000);

	return 0;
}

static void mtkts_dctm_unregister_thermal(void)
{
	mtkts_dctm_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtkts_dctm_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_dctm_read, NULL);
}

static const struct file_operations mtkts_dctm_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_dctm_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_dctm_write,
	.release = single_release,
};

static int tzdctm_cfg_matrix_read(struct seq_file *m, void *v)
{
	int i, j;

	/* dump array for AcMatrixNz */
	seq_printf(m, "%s array content :\n", customized_matrix[0]);
	for (i = 0 ; i < ACNZ ; i++)
		seq_printf(m, " [%d] = %lld\n", i, AcMatrixNz[i]);

	/* dump array for DcMatrix */
	seq_printf(m, "%s array content :\n", customized_matrix[1]);
	for (i = 0; i < NUMNODE; i++)
		for (j = 0; j < NUMRHS; j++)
			seq_printf(m, " [%d][%d] = %lld\n",
					i, j, DcMatrix[i][j]);

	/* dump array for CapMatrix */
	seq_printf(m, "%s array content :\n", customized_matrix[2]);
	for (i = 0 ; i < NUMNODE ; i++)
		seq_printf(m, " [%d] = %lld\n", i, CapMatrix[i]);

	return 0;
}

static ssize_t tzdctm_cfg_matrix_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[256];
	int len = 0;
	long long t_argv[18] = {0};
	int i, j;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	/*
	 * t_argv[0] ~ t_argv[8] : AcMatrixNz array content
	 * t_argv[9] ~ t_argv[14] : DcMatrix array content
	 * t_argv[15] ~ t_argv[17] : CapMatrix array content
	 */
	if (sscanf(desc,
		"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
		&t_argv[0], &t_argv[1], &t_argv[2], &t_argv[3], &t_argv[4],
		&t_argv[5], &t_argv[6], &t_argv[7], &t_argv[8], &t_argv[9],
		&t_argv[10], &t_argv[11], &t_argv[12], &t_argv[13], &t_argv[14],
		&t_argv[15], &t_argv[16], &t_argv[17]) == 18) {

		mtkts_dctm_printk(
			"%s input %s %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",
			__func__, customized_matrix[0],
			t_argv[0], t_argv[1], t_argv[2],
			t_argv[3], t_argv[4], t_argv[5],
			t_argv[6], t_argv[7], t_argv[8]);

		mtkts_dctm_printk(
			"%s input %s %lld %lld %lld %lld %lld %lld\n",
			__func__, customized_matrix[1],
			t_argv[9], t_argv[10], t_argv[11],
			t_argv[12], t_argv[13], t_argv[14]);

		mtkts_dctm_printk(
			"%s input %s %lld %lld %lld\n",
			__func__, customized_matrix[2],
			t_argv[15], t_argv[16], t_argv[17]);

		mutex_lock(&dctm_mutex);

		for (i = 0 ; i < ACNZ ; i++)
			AcMatrixNz[i] = t_argv[i];

		for (i = 0 ; i < NUMNODE ; i++)
			for (j = 0 ; j < NUMRHS ; j++)
				DcMatrix[i][j] = t_argv[i * NUMRHS + j + 9];

		for (i = 0 ; i < NUMNODE ; i++)
			CapMatrix[i] = t_argv[i + 15];

		mutex_unlock(&dctm_mutex);

		/* Reset DRC and related parameters */
		mtkts_dctm_drc_reset = 1;

		/* Reinit */
		tskinInit(mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP)/1000);

		return count;
	}

	mtkts_dctm_printk("%s bad argument\n", __func__);
	return -EINVAL;
}

static int tzdctm_cfg_matrix_open(struct inode *inode, struct file *file)
{
	return single_open(file, tzdctm_cfg_matrix_read, NULL);
}

static const struct file_operations tzdctm_cfg_matrix_fops = {
	.owner = THIS_MODULE,
	.open = tzdctm_cfg_matrix_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tzdctm_cfg_matrix_write,
	.release = single_release,
};

static int tzdctm_cfg_read(struct seq_file *m, void *v)
{
	seq_printf(m, "klog=%d, tamb=%d, tamb_coef=%d, tpcb_coef=%d, tpcbinit=%d\n",
			mtkts_dctm_debug_log, tamb,
			tamb_coef, tpcb_coef,  tpcbinit);

	return 0;
}

static ssize_t tzdctm_cfg_write(struct file *file, const char __user *buffer,
		size_t count,
		loff_t *data)
{
	char desc[64];
	int len = 0;
	int t_tamb = TAMB;
	int t_tamb_coef = TAMBCOEF;
	int t_tpcb_coef = TPCBCOEF;
	int t_tpcbinit = TPCBINIT;
	int klog_on = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d %d %d", &klog_on,
		&t_tamb, &t_tamb_coef, &t_tpcb_coef, &t_tpcbinit
		) == 5) {
		mtkts_dctm_printk("%s input %d %d %d %d\n",
			__func__, t_tamb, t_tamb_coef, t_tpcb_coef, t_tpcbinit);

		if (klog_on == 0 || klog_on == 1)
			mtkts_dctm_debug_log = klog_on;

		tamb = t_tamb;
		tamb_coef = t_tamb_coef;
		tpcb_coef = t_tpcb_coef;
		tpcbinit = t_tpcbinit;

		/* Reinit */
		tskinInit(mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP)/1000);

		return count;
	}
	mtkts_dctm_printk("%s bad argument\n", __func__);
	return -EINVAL;
}

static int tzdctm_cfg_open(struct inode *inode, struct file *file)
{
	return single_open(file, tzdctm_cfg_read, NULL);
}

static const struct file_operations tzdctm_cfg_fops = {
	.owner = THIS_MODULE,
	.open = tzdctm_cfg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tzdctm_cfg_write,
	.release = single_release,
};

static int tzdctm_drc_cfg_read(struct seq_file *m, void *v)
{
		seq_printf(m, "dctm_ttj_on=%d\n", mtkts_dctm_ttj_on);

		seq_printf(m,
			"init_r1_r2=%d, init_r1c1=%d, dtskin_thres=%d, drc_bound=%d\n",
			init_r1_r2, init_r1c1, dtskin_thres, drc_bound);

		seq_printf(m,
			"ttskin=%d, ave_window=%d, drc_ave_window=%d, ttj_limit=%d\n",
			ttskin, ave_window, drc_ave_window, ttj_limit);

		return 0;
}

static ssize_t tzdctm_drc_cfg_write(struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *data)
{
	char desc[64];
	int len = 0;

	int dctm_ttj_on = 0;
	int t_init_r1_r2 = INIT_R1_R2;
	int t_init_r1c1 = INIT_R1C1;
	int t_dtskin_thres = DTSKIN_THRES;
	int t_drc_bound = DRC_Bound;
	int t_ttskin = TTSKIN;
	int t_ave_window = AVE_WINDOW;
	int t_drc_ave_window = DRC_AVE_WINDOW;
	int t_ttj_limit = TTj_Limit;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d %d %d %d %d %d %d",
		&dctm_ttj_on,
		&t_init_r1_r2, &t_init_r1c1, &t_dtskin_thres, &t_drc_bound,
		&t_ttskin, &t_ave_window, &t_drc_ave_window, &t_ttj_limit
		) == 9) {

		mtkts_dctm_printk("%s dctm_ttj_on %d\n",
			__func__, dctm_ttj_on);

		if (dctm_ttj_on == 0 || dctm_ttj_on == 1)
			mtkts_dctm_ttj_on = dctm_ttj_on;

		mtkts_dctm_printk("%s input2 %d %d %d %d %d %d %d %d\n",
			__func__,
			t_init_r1_r2, t_init_r1c1, t_dtskin_thres, t_drc_bound,
			t_ttskin, t_ave_window, t_drc_ave_window, t_ttj_limit);

		init_r1_r2 = t_init_r1_r2;
		init_r1c1 = t_init_r1c1;
		dtskin_thres = t_dtskin_thres;
		drc_bound = t_drc_bound;
		ttskin = t_ttskin;
		ave_window = t_ave_window;
		drc_ave_window = t_drc_ave_window;
		ttj_limit = t_ttj_limit;

		/* Reset DRC and related parameters */
		mtkts_dctm_drc_reset = 1;

		/* Reinit */
		tskinInit(mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP)/1000);

		dctmdrc_update_params();

		return count;
	}
	mtkts_dctm_printk("%s bad argument\n", __func__);
	return -EINVAL;
}

static int tzdctm_drc_cfg_open(struct inode *inode, struct file *file)
{
	return single_open(file, tzdctm_drc_cfg_read, NULL);
}

static const struct file_operations tzdctm_drc_cfg_fops = {
	.owner = THIS_MODULE,
	.open = tzdctm_drc_cfg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tzdctm_drc_cfg_write,
	.release = single_release,
};

static int __init mtkts_dctm_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_dir = NULL;
#ifdef CONFIG_PM
	int ret = -1;
#endif
	mtkts_dctm_printk("[%s]\n", __func__);

	mtkts_dir = mtk_thermal_get_proc_drv_therm_dir_entry();

	if (!mtkts_dir) {
		mtkts_dctm_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
			__func__);
	} else {
		entry =
		    proc_create("tzdctm", 0664, mtkts_dir, &mtkts_dctm_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("tzdctm_cfg", 0664, mtkts_dir,
			&tzdctm_cfg_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("tzdctm_drc_cfg", 0664, mtkts_dir,
			&tzdctm_drc_cfg_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("tzdctm_cfg_matrix", 0664, mtkts_dir,
			&tzdctm_cfg_matrix_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	mtkts_dctm_drc_reset = 1;
	tskinInit(tpcbinit);

	mtkts_dctm_register_thermal();
#if 0
		mtkTTimer_register("mtktsdctm",
			mtkts_dctm_start_thermal_timer,
			mtkts_dctm_cancel_thermal_timer);
#endif
#ifdef CONFIG_PM
	ret = register_pm_notifier(&dctm_pm_notifier_func);
	if (ret)
		pr_notice("Failed to register dctm PM notifier.\n");
#endif
	return 0;
}

static void __exit mtkts_dctm_exit(void)
{
	mtkts_dctm_dprintk("[mtkts_dctm_exit]\n");
	mtkts_dctm_unregister_thermal();
#if 0
	mtkTTimer_unregister("mtktsdctm");
#endif
}

module_init(mtkts_dctm_init);
module_exit(mtkts_dctm_exit);
