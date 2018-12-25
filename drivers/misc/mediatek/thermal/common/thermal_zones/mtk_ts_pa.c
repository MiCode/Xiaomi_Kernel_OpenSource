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
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include "mt-plat/mtk_mdm_monitor.h"
#include <linux/uidgid.h>
#include <linux/slab.h>
#include <mtk_ts_setting.h>

#if Feature_Thro_update
/* For using net dev + */
#include <linux/netdevice.h>
/* For using net dev - */
#include <linux/timer.h>
#endif

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);
static int isTimerCancelled;

static unsigned int interval;	/* seconds, 0 : no auto polling */
static unsigned int trip_temp[10] = { 85000, 80000, 70000, 60000, 50000,
					40000, 30000, 20000, 10000, 5000 };

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;
static struct thermal_cooling_device *cl_dev_sysrst;
static int mtktspa_debug_log;
static int kernelmode;

static int num_trip;
static char g_bind0[20] = "mtktspa-sysrst";
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

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

#define mtktspa_TEMP_CRIT 85000	/* 85.000 degree Celsius */

#define mtktspa_dprintk(fmt, args...)   \
do {                                    \
	if (mtktspa_debug_log) {                \
		pr_debug("[Thermal/TZ/PA]" fmt, ##args); \
	}                                   \
} while (0)


#if Feature_Thro_update
struct pa_stats {
	unsigned long pre_time;
	unsigned long pre_tx_bytes;
};

static struct pa_stats pa_stats_info;
static struct timer_list pa_stats_timer;
static unsigned long pre_time;
static unsigned long tx_throughput;

static unsigned long get_tx_bytes(void)
{
	struct net_device *dev;
	struct net *net;
	unsigned long tx_bytes = 0;

	read_lock(&dev_base_lock);
	for_each_net(net) {
		for_each_netdev(net, dev) {
			if (!strncmp(dev->name, "ccmni", 5)) {
				struct rtnl_link_stats64 temp;
				const struct rtnl_link_stats64 *stats =
						dev_get_stats(dev, &temp);
				/* mtktspa_dprintk("%s tx_bytes: %lu\n",
				 * dev->name, (unsigned long)stats->tx_bytes);
				 */
				tx_bytes = tx_bytes + stats->tx_bytes;
			}
		}
	}
	read_unlock(&dev_base_lock);
	return tx_bytes;
}

int tspa_get_MD_tx_tput(void)
{
	return tx_throughput;
}

static int pa_cal_stats(unsigned long data)
{
	struct pa_stats *stats_info = (struct pa_stats *) data;
	struct timeval cur_time;

	mtktspa_dprintk("[%s] pre_time=%lu, pre_data=%lu\n", __func__,
					pre_time, stats_info->pre_tx_bytes);

	do_gettimeofday(&cur_time);

	if (pre_time != 0 && cur_time.tv_sec > pre_time) {
		unsigned long tx_bytes = get_tx_bytes();

		if (tx_bytes > stats_info->pre_tx_bytes) {

			tx_throughput = ((tx_bytes - stats_info->pre_tx_bytes)
					/ (cur_time.tv_sec - pre_time)) >> 7;

			mtktspa_dprintk(
				"[%s] cur_time=%lu, cur_data=%lu, tx_throughput=%luKb/s\n",
				__func__, cur_time.tv_sec, tx_bytes,
				tx_throughput);

			stats_info->pre_tx_bytes = tx_bytes;
		} else if (tx_bytes < stats_info->pre_tx_bytes) {
			/* Overflow */
			tx_throughput = ((0xffffffff - stats_info->pre_tx_bytes
					+ tx_bytes) /
					(cur_time.tv_sec - pre_time)) >> 7;

			stats_info->pre_tx_bytes = tx_bytes;
			mtktspa_dprintk("[%s] cur_tx(%lu) < pre_tx\n",
							__func__, tx_bytes);
		} else {
			/* No traffic */
			tx_throughput = 0;
			mtktspa_dprintk("[%s] cur_tx(%lu) = pre_tx\n",
							__func__, tx_bytes);
		}
	} else {
		/* Overflow possible ??*/
		tx_throughput = 0;
		mtktspa_dprintk("[%s] cur_time(%lu) < pre_time\n",
						__func__, cur_time.tv_sec);
	}

	pre_time = cur_time.tv_sec;
	mtktspa_dprintk("[%s] pre_time=%lu, tv_sec=%lu\n", __func__,
						pre_time, cur_time.tv_sec);

	pa_stats_timer.expires = jiffies + 1 * HZ;
	add_timer(&pa_stats_timer);
	return 0;
}
#endif


/*
 *struct md_info{
 *		char *attribute;
 *		int value;
 *		char *unit;
 *		int invalid_value;
 *		int index;
 *};
 *struct md_info g_pinfo_list[] =
 *{{"TXPWR_MD1", -127, "db", -127, 0},
 * {"TXPWR_MD2", -127, "db", -127, 1},
 * {"RFTEMP_2G_MD1", -32767, "¢XC", -32767, 2},
 * {"RFTEMP_2G_MD2", -32767, "¢XC", -32767, 3},
 * {"RFTEMP_3G_MD1", -32767, "¢XC", -32767, 4},
 * {"RFTEMP_3G_MD2", -32767, "¢XC", -32767, 5}};
 */
static DEFINE_MUTEX(TSPA_lock);
static int mtktspa_get_hw_temp(void)
{
	struct md_info *p_info;
	int size, i;

	mutex_lock(&TSPA_lock);
	mtk_mdm_get_md_info(&p_info, &size);
	for (i = 0; i < size; i++) {
		mtktspa_dprintk(
			"PA temperature: name:%s, value:%d, invalid_value=%d\n",
			p_info[i].attribute, p_info[i].value,
			p_info[i].invalid_value);

		if (!strcmp(p_info[i].attribute, "RFTEMP_2G_MD1")) {
			mtktspa_dprintk("PA temperature: RFTEMP_2G_MD1\n");
			if (p_info[i].value != p_info[i].invalid_value)
				break;
		} else if (!strcmp(p_info[i].attribute, "RFTEMP_3G_MD1")) {
			mtktspa_dprintk("PA temperature: RFTEMP_3G_MD1\n");
			if (p_info[i].value != p_info[i].invalid_value)
				break;
		}
	}

	if (i == size) {
		mtktspa_dprintk("PA temperature: not ready\n");
		mutex_unlock(&TSPA_lock);
		return -127000;
	}

	mtktspa_dprintk("PA temperature: %d\n", p_info[i].value);

	if ((p_info[i].value > 100000) || (p_info[i].value < -30000))
		pr_debug("[Power/PA_Thermal] PA T=%d\n", p_info[i].value);
	mutex_unlock(&TSPA_lock);
	return p_info[i].value;
}

static int mtktspa_get_temp(struct thermal_zone_device *thermal, int *t)
{
	*t = mtktspa_get_hw_temp();

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtktspa_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspa_dprintk("[mtktspa_bind] %s\n", cdev->type);
	} else
		return 0;


	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktspa_dprintk("[mtktspa_bind] error binding cooling dev\n");
		return -EINVAL;
	}

	mtktspa_dprintk("[mtktspa_bind] binding OK\n");
	return 0;
}

static int mtktspa_unbind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspa_dprintk("[mtktspa_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktspa_dprintk(
			"[mtktspa_unbind] error unbinding cooling dev\n");

		return -EINVAL;
	}

	mtktspa_dprintk("[mtktspa_unbind] unbinding OK\n");
	return 0;
}

static int mtktspa_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;

	return 0;
}

static int mtktspa_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktspa_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktspa_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktspa_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = mtktspa_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktspa_dev_ops = {
	.bind = mtktspa_bind,
	.unbind = mtktspa_unbind,
	.get_temp = mtktspa_get_temp,
	.get_mode = mtktspa_get_mode,
	.set_mode = mtktspa_set_mode,
	.get_trip_type = mtktspa_get_trip_type,
	.get_trip_temp = mtktspa_get_trip_temp,
	.get_crit_temp = mtktspa_get_crit_temp,
};

/*
 * cooling device callback functions (mtktspa_cooling_sysrst_ops)
 * 1 : ON and 0 : OFF
 */
static int tspa_sysrst_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int tspa_sysrst_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int tspa_sysrst_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{

	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_debug("Power/PA_Thermal: reset, reset, reset!!!");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		pr_debug("*****************************************");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		*(unsigned int *)0x0 = 0xdead;
	}
	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtktspa_cooling_sysrst_ops = {
	.get_max_state = tspa_sysrst_get_max_state,
	.get_cur_state = tspa_sysrst_get_cur_state,
	.set_cur_state = tspa_sysrst_set_cur_state,
};

static int mtktspa_register_thermal(void);
static void mtktspa_unregister_thermal(void);

static int mtktspa_read(struct seq_file *m, void *v)
{
	seq_printf(m,
		"[ mtktspa_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\n",
		trip_temp[0], trip_temp[1], trip_temp[2],
		trip_temp[3], trip_temp[4]);

	seq_printf(m,
		"trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[5], trip_temp[6], trip_temp[7],
		trip_temp[8], trip_temp[9]);

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

static ssize_t mtktspa_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	int i;
	struct mtktspa_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktspa_data *ptr_mtktspa_data = kmalloc(
					sizeof(*ptr_mtktspa_data), GFP_KERNEL);

	if (ptr_mtktspa_data == NULL) {
		/* pr_debug("[%s] kmalloc fail\n\n", __func__); */
		return -ENOMEM;
	}

	len = (count < (sizeof(ptr_mtktspa_data->desc) - 1)) ?
				count : (sizeof(ptr_mtktspa_data->desc) - 1);

	if (copy_from_user(ptr_mtktspa_data->desc, buffer, len)) {
		kfree(ptr_mtktspa_data);
		return 0;
	}

	ptr_mtktspa_data->desc[len] = '\0';

	if (sscanf(ptr_mtktspa_data->desc,
		"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktspa_data->trip[0], &ptr_mtktspa_data->t_type[0],
		ptr_mtktspa_data->bind0,
		&ptr_mtktspa_data->trip[1], &ptr_mtktspa_data->t_type[1],
		ptr_mtktspa_data->bind1,
		&ptr_mtktspa_data->trip[2], &ptr_mtktspa_data->t_type[2],
		ptr_mtktspa_data->bind2,
		&ptr_mtktspa_data->trip[3], &ptr_mtktspa_data->t_type[3],
		ptr_mtktspa_data->bind3,
		&ptr_mtktspa_data->trip[4], &ptr_mtktspa_data->t_type[4],
		ptr_mtktspa_data->bind4,
		&ptr_mtktspa_data->trip[5], &ptr_mtktspa_data->t_type[5],
		ptr_mtktspa_data->bind5,
		&ptr_mtktspa_data->trip[6], &ptr_mtktspa_data->t_type[6],
		ptr_mtktspa_data->bind6,
		&ptr_mtktspa_data->trip[7], &ptr_mtktspa_data->t_type[7],
		ptr_mtktspa_data->bind7,
		&ptr_mtktspa_data->trip[8], &ptr_mtktspa_data->t_type[8],
		ptr_mtktspa_data->bind8,
		&ptr_mtktspa_data->trip[9], &ptr_mtktspa_data->t_type[9],
		ptr_mtktspa_data->bind9,
		&ptr_mtktspa_data->time_msec) == 32) {

		down(&sem_mutex);
		mtktspa_dprintk("[mtktspa_write] mtktspa_unregister_thermal\n");
		mtktspa_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT, "mtktspa_write",
						"Bad argument");
			#endif
			mtktspa_dprintk("[mtktspa_write] bad argument\n");
			kfree(ptr_mtktspa_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktspa_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktspa_data->bind0[i];
			g_bind1[i] = ptr_mtktspa_data->bind1[i];
			g_bind2[i] = ptr_mtktspa_data->bind2[i];
			g_bind3[i] = ptr_mtktspa_data->bind3[i];
			g_bind4[i] = ptr_mtktspa_data->bind4[i];
			g_bind5[i] = ptr_mtktspa_data->bind5[i];
			g_bind6[i] = ptr_mtktspa_data->bind6[i];
			g_bind7[i] = ptr_mtktspa_data->bind7[i];
			g_bind8[i] = ptr_mtktspa_data->bind8[i];
			g_bind9[i] = ptr_mtktspa_data->bind9[i];
		}

		mtktspa_dprintk(
			"[mtktspa_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		mtktspa_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtktspa_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtktspa_dprintk(
			"[mtktspa_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		mtktspa_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktspa_data->trip[i];


		interval = ptr_mtktspa_data->time_msec / 1000;

		mtktspa_dprintk(
			"[mtktspa_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		mtktspa_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6],
			trip_temp[7], trip_temp[8]);

		mtktspa_dprintk("trip_9_temp=%d,time_ms=%d\n",
						trip_temp[9], interval * 1000);

		mtktspa_dprintk("[mtktspa_write] mtktspa_register_thermal\n");
		mtktspa_register_thermal();
		up(&sem_mutex);

		kfree(ptr_mtktspa_data);
		return count;
	}
	#ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
							"mtktspa_write",
							"Bad argument");
    #endif
	mtktspa_dprintk("[mtktspa_write] bad argument\n");
	kfree(ptr_mtktspa_data);
	return -EINVAL;

}

static int mtktspa_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktspa_read, NULL);
}

static const struct file_operations mtktspa_fops = {
	.owner = THIS_MODULE,
	.open = mtktspa_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktspa_write,
	.release = single_release,
};

#if Feature_Thro_update
int pa_mobile_tx_thro_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%lu\n", tx_throughput);

	mtktspa_dprintk("[%s] tx=%lu\n", __func__, tx_throughput);
	return 0;
}

static int pa_mobile_tx_thro_open(struct inode *inode, struct file *file)
{
	return single_open(file, pa_mobile_tx_thro_read, PDE_DATA(inode));
}

static const struct file_operations _tx_thro_fops = {
	.owner = THIS_MODULE,
	.open = pa_mobile_tx_thro_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static void mtkts_pa_cancel_thermal_timer(void)
{
	/* cancel timer */
	/* pr_debug("mtkts_pa_cancel_thermal_timer\n"); */

	/* stop thermal framework polling when entering deep idle */
	if (down_trylock(&sem_mutex))
		return;

	if (thz_dev) {
		cancel_delayed_work(&(thz_dev->poll_queue));
		isTimerCancelled = 1;
	}

	up(&sem_mutex);
}


static void mtkts_pa_start_thermal_timer(void)
{
	/* pr_debug("mtkts_pa_start_thermal_timer\n"); */
	/* resume thermal framework polling when leaving deep idle */

	if (!isTimerCancelled)
		return;



	if (down_trylock(&sem_mutex))
		return;

	if (thz_dev != NULL && interval != 0) {
		mod_delayed_work(system_freezable_power_efficient_wq,
					&(thz_dev->poll_queue),
					round_jiffies(msecs_to_jiffies(3000)));
		isTimerCancelled = 0;
	}
	up(&sem_mutex);
}


int mtktspa_register_cooler(void)
{
	/* cooling devices */
	cl_dev_sysrst = mtk_thermal_cooling_device_register(
						"mtktspa-sysrst", NULL,
						&mtktspa_cooling_sysrst_ops);
	return 0;
}

static int mtktspa_register_thermal(void)
{
	mtktspa_dprintk("[mtktspa_register_thermal]\n");

	/* trips */
	thz_dev = mtk_thermal_zone_device_register(
						"mtktspa", num_trip, NULL,
						&mtktspa_dev_ops, 0, 0, 0,
						interval * 1000);

	mtk_mdm_set_md1_signal_period(interval);

	return 0;
}

void mtktspa_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

static void mtktspa_unregister_thermal(void)
{
	mtktspa_dprintk("[mtktspa_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int __init mtktspa_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktspa_dir = NULL;
#if Feature_Thro_update
	struct proc_dir_entry *mobile_thro_proc_dir = NULL;
#endif

	mtktspa_dprintk("[%s]\n", __func__);

	err = mtktspa_register_cooler();
	if (err)
		return err;

	err = mtktspa_register_thermal();
	if (err)
		goto err_unreg;

#if Feature_Thro_update
	mobile_thro_proc_dir = proc_mkdir("mobile_tm", NULL);

	if (!mobile_thro_proc_dir)
		mtktspa_dprintk(
			"[mobile_tm_proc_register]: mkdir /proc/mobile_tm failed\n");
	else
		proc_create("tx_thro", 0644, mobile_thro_proc_dir,
							&_tx_thro_fops);
#endif

	mtktspa_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktspa_dir) {
		mtktspa_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzpa", 0664, mtktspa_dir, &mtktspa_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

#if Feature_Thro_update
	/* init a timer for stats tx bytes */
	pa_stats_info.pre_time = 0;
	pa_stats_info.pre_tx_bytes = 0;

	init_timer_deferrable(&pa_stats_timer);
	pa_stats_timer.function = (void *)&pa_cal_stats;
	pa_stats_timer.data = (unsigned long) &pa_stats_info;
	pa_stats_timer.expires = jiffies + 1 * HZ;
	add_timer(&pa_stats_timer);
#endif

	mtkTTimer_register("mtktspa", mtkts_pa_start_thermal_timer,
						mtkts_pa_cancel_thermal_timer);

	return 0;

err_unreg:
	mtktspa_unregister_cooler();
	return err;
}

static void __exit mtktspa_exit(void)
{
	mtktspa_dprintk("[mtktspa_exit]\n");
	mtktspa_unregister_thermal();
	mtktspa_unregister_cooler();

	mtkTTimer_unregister("mtktspa");
#if Feature_Thro_update
	del_timer(&pa_stats_timer);
#endif
}
module_init(mtktspa_init);
module_exit(mtktspa_exit);
