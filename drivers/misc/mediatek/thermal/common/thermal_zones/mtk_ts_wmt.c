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
#include <linux/thermal.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
/* #include "wmt_tm.h" */
#include <mt-plat/mtk_thermal_monitor.h>
#include <linux/timer.h>
#include <linux/pid.h>
/* For using net dev + */
#include <linux/netdevice.h>
/* For using net dev - */
#include <mt-plat/mtk_wcn_cmb_stub.h>
#include <mt-plat/aee.h>
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include <linux/uidgid.h>
#include <linux/slab.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
/*=============================================================
 *Weak functions
 *=============================================================
 */
int __attribute__ ((weak))
mtk_wcn_cmb_stub_query_ctrl(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}
/*=============================================================*/
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);
static int isTimerCancelled;

static int wmt_tm_debug_log;
#define wmt_tm_dprintk(fmt, args...)   \
do { \
	if (wmt_tm_debug_log) \
		pr_debug("[thermal/wmt]" fmt, ##args); \
} while (0)

#define wmt_tm_printk(fmt, args...)   \
pr_debug("[thermal/wmt]" fmt, ##args)

#define wmt_tm_info(fmt, args...)   \
pr_debug("[thermal/wmt]" fmt, ##args)

struct linux_thermal_ctrl_if {
	int kernel_mode;
	int interval;
	struct thermal_zone_device *thz_dev;
	struct thermal_cooling_device *cl_dev;
	struct thermal_cooling_device *cl_pa1_dev;
	struct thermal_cooling_device *cl_pa2_dev;
};

struct wmt_tm_t {
	struct linux_thermal_ctrl_if linux_if;
};

struct wmt_stats {
	unsigned long pre_time;
	unsigned long pre_tx_bytes;
};

#define NR_TS_SENSORS	(4)
static int (*ts_get_temp_wrap[4]) (void) = {
	mtk_wcn_cmb_stub_query_ctrl /* 0 is for WMT sensor */
	, get_immediate_ts1_wrap
	, get_immediate_ts2_wrap
	, get_immediate_ts3_wrap};

static struct timer_list wmt_stats_timer;
static struct wmt_stats wmt_stats_info;
static unsigned long pre_time;
static unsigned long tx_throughput;

/* New Wifi throttling Algo+ */
/* over_up_time * polling interval > up_duration --> throttling */
static unsigned int over_up_time;	/* polling time */
static unsigned int up_duration = 30;	/* sec */
static unsigned int up_denominator = 2;
static unsigned int up_numerator = 1;

/* below_low_time * polling interval > low_duration --> throttling */
static unsigned int below_low_time;	/* polling time */
static unsigned int low_duration = 10;	/* sec */
static unsigned int low_denominator = 2;
static unsigned int low_numerator = 3;

static unsigned int low_rst_time;
static unsigned int low_rst_max = 3;
/* New Wifi throttling Algo- */

#define MAX_LEN	(256)
#define COOLER_THRO_NUM (3)
#define COOLER_NUM (10)
#define ONE_MBITS_PER_SEC (1000)

static unsigned int tm_pid;
static unsigned int tm_input_pid;
static unsigned int tm_wfd_stat;
/* static unsigned int wifi_in_soc = 0; */
static struct task_struct *pg_task;

/* + Cooler info + */
static int g_num_trip;
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

/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 * use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 5;
static int polling_factor2 = 10;

static unsigned int cl_dev_state;
static unsigned int cl_pa1_dev_state;
static unsigned int cl_pa2_dev_state;
static unsigned int g_trip_temp[COOLER_NUM] = { 125000, 115000, 105000, 85000,
							0, 0, 0, 0, 0, 0 };

/* static int g_thro[COOLER_THRO_NUM] =
 *	{10 * ONE_MBITS_PER_SEC, 5 * ONE_MBITS_PER_SEC, 1 * ONE_MBITS_PER_SEC};
 */
static int g_thermal_trip[COOLER_NUM] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
/* - Cooler info - */

static struct wmt_tm_t g_wmt_tm;
static struct wmt_tm_t *pg_wmt_tm = &g_wmt_tm;

#define init_wifi_tput_ratio (100)

/* Interal usage */
/* these two are in milli degree C */
static int g_curr_temp;
static int g_prev_temp;
static int wifi_target_tj = 120000;
static int wifi_target_offset = 1000;
static int cpu_wifi_target_tj_offset = 10000;
static int WIFI_TARGET_TJ_HIGH;
static int WIFI_TARGET_TJ_LOW;
static int g_limit_tput = -1;
static unsigned int cl_dev_adp_cpu_state_active;

/* parameters from adb shell */
/*	0: for old WiFi algorithm.
 *	1: for HRA/ATR
 */
static int wifi_throttle_version = 1;
static int wmt_wifi_target_tj = 120000;
static int wmt_wifi_target_offset = 1000;
static int tj_stable_range = 1000;
/*	0: wifi last (default)
 *	1: wifi first
 *	2: no throttle wifi
 *	3: independent
 *	4: not share target tj
 */
static int resource_allocator_policy = 4;
static int min_wifi_tput_ratio = 40;
static int max_wifi_tput_ratio = 200;
static int min_wifi_tput = 1000;
/* initial value: assume 1 degreeC for temp.
 *		<=> 1 unit for wifi_tput_ratio(0~100)
 */
static int tt_wifi_high = 50;
static int tt_wifi_low = 50;
static int tp_wifi_rise = 10000;
static int tp_wifi_fall = 10000;
static int triggered; /* wt2 */
static int sensor_select; /* select TS, 0=WMT, 1=TS1, 2=TS2, 3=TS3 */

static inline int is_wifi_tput_min(void)
{
	wmt_tm_dprintk("%s: %d\n", __func__, __LINE__);
	return (g_limit_tput != -1 && g_limit_tput <= min_wifi_tput) ? 1 : 0;
}

static int wmt_send_signal(int level)
{
	int ret = 0;
	int thro = level;

	g_limit_tput = level;
	wmt_tm_dprintk("%s +++ level %d\n", __func__, level);

	if (tm_input_pid == 0) {
		wmt_tm_dprintk("[%s] pid is empty\n", __func__);
		ret = -1;
	}

	wmt_tm_printk("[%s] pid is %d, %d, %d\n", __func__,
						tm_pid, tm_input_pid, thro);

	if (ret == 0 && tm_input_pid != tm_pid) {
		tm_pid = tm_input_pid;

		if (pg_task != NULL)
			put_task_struct(pg_task);
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_code = 4;
		info.si_errno = thro;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		wmt_tm_info("[%s] ret=%d\n", __func__, ret);

	return ret;
}

static unsigned long
set_adaptive_wifi_tput_limit(unsigned int cur_tput, unsigned int limit_ratio)
{
	int limit_tput;
	static int prev_limit_tput = -1, limit_upper_bound = -1;

	if (limit_ratio == -1) {
		prev_limit_tput = -1;
		limit_tput = -1;
	} else {
		/* when we enter cooler */
		if (prev_limit_tput == -1) {
			prev_limit_tput = cur_tput;
			limit_upper_bound = cur_tput;
		}
		limit_tput = (prev_limit_tput * limit_ratio)
							/ init_wifi_tput_ratio;

		if (limit_upper_bound >= min_wifi_tput)
			limit_tput = clamp(limit_tput, min_wifi_tput,
							limit_upper_bound);
		else
			limit_tput = -1;


		prev_limit_tput = limit_tput;
	}

	wmt_tm_dprintk("%s: Curr Tput=%lu, ratio=%u, limit_tput=%d"
		, __func__, tx_throughput
		, limit_ratio, limit_tput);
	wmt_send_signal(limit_tput);

	return limit_tput;
}

static int adaptive_tput_ratio(long prev_temp, long curr_temp)
{
	static int wifi_tput_ratio;

	WIFI_TARGET_TJ_HIGH = wifi_target_tj + tj_stable_range;
	WIFI_TARGET_TJ_LOW = wifi_target_tj - tj_stable_range;

	wmt_tm_dprintk("%s: active=%d tirgger=%d\n"
		, __func__, cl_dev_adp_cpu_state_active, triggered);

	if (cl_dev_adp_cpu_state_active == 1) {
		int tt_wifi = wifi_target_tj - curr_temp; /* unit: mC */
		int tp_wifi = prev_temp - curr_temp; /* unit: mC */

		/* Check if it is triggered */
		if (!triggered) {
			if (curr_temp < wifi_target_tj)
				return 0;
			triggered = 1;
		}
		wifi_tput_ratio = init_wifi_tput_ratio;
		/* Adjust total power budget if necessary */
		if (((curr_temp > WIFI_TARGET_TJ_HIGH)
			&& (curr_temp > prev_temp)) ||
			((curr_temp <= WIFI_TARGET_TJ_HIGH)
				&& (curr_temp >= WIFI_TARGET_TJ_LOW)
		    && (curr_temp >= (prev_temp + (tj_stable_range * 2))))) {

			wifi_tput_ratio += (tt_wifi / tt_wifi_high +
							tp_wifi / tp_wifi_rise);

		} else if (((curr_temp > WIFI_TARGET_TJ_HIGH)
			&& (curr_temp <= prev_temp)) ||
			((curr_temp <= WIFI_TARGET_TJ_HIGH)
			&& (curr_temp >= WIFI_TARGET_TJ_LOW)
			&& (curr_temp <= (prev_temp -
						(tj_stable_range * 2))))) {

			wifi_tput_ratio += (tt_wifi / tt_wifi_high +
							tp_wifi / tp_wifi_fall);

		} else if ((curr_temp < WIFI_TARGET_TJ_LOW)
			&& (curr_temp > prev_temp)) {

			wifi_tput_ratio += (tt_wifi / tt_wifi_low +
							tp_wifi / tp_wifi_rise);

		} else if ((curr_temp < WIFI_TARGET_TJ_LOW)
			&& (curr_temp <= prev_temp)) {

			wifi_tput_ratio += (tt_wifi / tt_wifi_low +
							tp_wifi / tp_wifi_fall);
		}

		wifi_tput_ratio =
		    (wifi_tput_ratio > min_wifi_tput_ratio) ?
					wifi_tput_ratio : min_wifi_tput_ratio;

		wifi_tput_ratio =
		    (wifi_tput_ratio < max_wifi_tput_ratio) ?
					wifi_tput_ratio : max_wifi_tput_ratio;

		wmt_tm_dprintk(
			"%s Wifi TJ %d Tp %ld, Tc %ld, wifi_tput_ratio %d tput %lu\n",
			__func__, wifi_target_tj, prev_temp, curr_temp,
			wifi_tput_ratio, tx_throughput);

		set_adaptive_wifi_tput_limit(tx_throughput, wifi_tput_ratio);
	} else {
		if (triggered) {
			triggered = 0;
			wmt_tm_dprintk("%s :unlimit Tp %ld Tc %ld Pt %lu\n",
				__func__, prev_temp, curr_temp, tx_throughput);

			set_adaptive_wifi_tput_limit(0, -1);
		}
	}

	return 0;
}

/* extern int cpu_target_tj; // in mtk_ts_cpu.c */
/* extern int cpu_target_offset; // in mtk_ts_cpu.c */
/**
 * @temp	current temperature in milli degree C
 */
static void heterogeneous_resource_allocator(int temp)
{
	wmt_tm_dprintk("%s: wifi_throttle_version=%d, wifi temp=%d\n"
		, __func__, wifi_throttle_version, temp);

	if (wifi_throttle_version == 0) {
		return;	/* for old  WiFi throttle */
	} else if (wifi_throttle_version == 1) {
		wmt_tm_dprintk(
			"%s: temp= %d policy= %d target_tj= %d target_offset= %d\n",
			__func__, temp, resource_allocator_policy,
			wifi_target_tj, wifi_target_offset);

		switch (resource_allocator_policy) {
		case 0:	/* wifi last */
			if (is_cpu_power_unlimit()) {
				wifi_target_tj = wmt_wifi_target_tj;
				wifi_target_offset = wmt_wifi_target_offset;
				cl_dev_adp_cpu_state_active =
				    (temp >= (wifi_target_tj
						- wifi_target_offset)) ? 1 : 0;

				adaptive_tput_ratio(g_prev_temp, temp);
			} else {
				if (!is_wifi_tput_min()) {
					set_adaptive_wifi_tput_limit(
							tx_throughput,
							min_wifi_tput_ratio);
				}
				triggered = 1;	/* wt2 */
			}
			break;

		case 1:	/* wifi first */
			break;

		case 2:	/* no throttle wifi */
			if (g_limit_tput != -1)
				set_adaptive_wifi_tput_limit(0, -1);
			break;

		case 3:	/* independent */
			wifi_target_offset = wmt_wifi_target_offset;
			if (is_cpu_power_unlimit())
				wifi_target_tj = wmt_wifi_target_tj;
			/* min(wmt_wifi_target_tj, get_cpu_target_tj()
			 *			- cpu_wifi_target_tj_offset);
			 */
			else
				wifi_target_tj =
				    min(wmt_wifi_target_tj,
					get_cpu_target_tj() -
						cpu_wifi_target_tj_offset);

			cl_dev_adp_cpu_state_active =
			    (temp >= (wifi_target_tj - wifi_target_offset)) ?
									1 : 0;

			adaptive_tput_ratio(g_prev_temp, temp);
			break;
		case 4:	/* not share target tj (default):
			 * for non-integrated WiFi
			 */
		default:
			wifi_target_tj = wmt_wifi_target_tj;
			wifi_target_offset = wmt_wifi_target_offset;
			cl_dev_adp_cpu_state_active =
			    (temp >= (wifi_target_tj - wifi_target_offset)) ?
									1 : 0;
			adaptive_tput_ratio(g_prev_temp, temp);
			break;
		}
	}
}

static unsigned long get_tx_bytes(void)
{
	struct net_device *dev;
	struct net *net;
	unsigned long tx_bytes = 0;

	read_lock(&dev_base_lock);
	for_each_net(net) {
		for_each_netdev(net, dev) {
			if (!strncmp(dev->name, "wlan", 4)
				|| !strncmp(dev->name, "ap", 2)
				|| !strncmp(dev->name, "p2p", 3)) {

				struct rtnl_link_stats64 temp;
				const struct rtnl_link_stats64 *stats =
						dev_get_stats(dev, &temp);

				tx_bytes = tx_bytes + stats->tx_bytes;
			}
		}
	}
	read_unlock(&dev_base_lock);
	return tx_bytes;
}

int tswmt_get_WiFi_tx_tput(void)
{
	return tx_throughput;
}

static void wmt_cal_stats(unsigned long data)
{
	struct wmt_stats *stats_info = (struct wmt_stats *)data;
	struct timeval cur_time;

	wmt_tm_dprintk("[%s] pre_time=%lu, pre_data=%lu\n", __func__, pre_time,
		       stats_info->pre_tx_bytes);

	do_gettimeofday(&cur_time);

	if (pre_time != 0 && cur_time.tv_sec > pre_time) {
		unsigned long tx_bytes = get_tx_bytes();

		if (tx_bytes > stats_info->pre_tx_bytes) {

			tx_throughput =
			    ((tx_bytes - stats_info->pre_tx_bytes)
					/ (cur_time.tv_sec - pre_time)) >> 7;

			wmt_tm_dprintk(
				"[%s] cur_time=%lu, cur_data=%lu, tx_throughput=%luK bit/s(%luK Byte/s)\n",
				__func__, cur_time.tv_sec, tx_bytes,
				tx_throughput, tx_throughput >> 3);

			stats_info->pre_tx_bytes = tx_bytes;
		} else if (tx_bytes < stats_info->pre_tx_bytes) {
			/* Overflow */
			tx_throughput = ((0xffffffff -
					stats_info->pre_tx_bytes + tx_bytes)
					/ (cur_time.tv_sec - pre_time)) >> 7;

			stats_info->pre_tx_bytes = tx_bytes;
			wmt_tm_dprintk("[%s] cur_tx(%lu) < pre_tx\n", __func__,
								tx_bytes);
		} else {
			/* No traffic */
			tx_throughput = 0;
			wmt_tm_dprintk("[%s] cur_tx(%lu) = pre_tx\n", __func__,
								tx_bytes);
		}
	} else {
		/* Overflow possible ?? */
		tx_throughput = 0;
		wmt_tm_printk("[%s] cur_time(%lu) < pre_time\n", __func__,
							cur_time.tv_sec);
	}

	pre_time = cur_time.tv_sec;
	wmt_tm_dprintk("[%s] pre_time=%lu, tv_sec=%lu\n", __func__,
						pre_time, cur_time.tv_sec);

	wmt_stats_timer.expires = jiffies + 1 * HZ;
	add_timer(&wmt_stats_timer);
}

static int wmt_thz_bind(struct thermal_zone_device *thz_dev,
			struct thermal_cooling_device *cool_dev)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;
	int table_val = 0;

	wmt_tm_dprintk("%s\n", __func__);

	if (pg_wmt_tm)
		p_linux_if = &pg_wmt_tm->linux_if;
	else
		return -EINVAL;

	if (!strcmp(cool_dev->type, g_bind0)) {
		table_val = 0;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind1)) {
		table_val = 1;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind2)) {
		table_val = 2;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind3)) {
		table_val = 3;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind4)) {
		table_val = 4;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind5)) {
		table_val = 5;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind6)) {
		table_val = 6;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind7)) {
		table_val = 7;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind8)) {
		table_val = 8;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind9)) {
		table_val = 9;
		wmt_tm_dprintk("[%s] %s\n", __func__, cool_dev->type);
	} else
		return 0;

	if (mtk_thermal_zone_bind_cooling_device(
		thz_dev, table_val, cool_dev)) {
		wmt_tm_info("%s binding fail\n", __func__);
		return -EINVAL;
	}

	wmt_tm_dprintk("%s binding OK\n", __func__);
	return 0;
}

static int wmt_thz_unbind(struct thermal_zone_device *thz_dev,
			  struct thermal_cooling_device *cool_dev)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;
	int table_val = 0;

	wmt_tm_dprintk("%s\n", __func__);

	if (pg_wmt_tm)
		p_linux_if = &pg_wmt_tm->linux_if;
	else
		return -EINVAL;

	if (!strcmp(cool_dev->type, g_bind0)) {
		table_val = 0;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind1)) {
		table_val = 1;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind2)) {
		table_val = 2;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind3)) {
		table_val = 3;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind4)) {
		table_val = 4;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind5)) {
		table_val = 5;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind6)) {
		table_val = 6;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind7)) {
		table_val = 7;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind8)) {
		table_val = 8;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else if (!strcmp(cool_dev->type, g_bind9)) {
		table_val = 9;
		wmt_tm_dprintk("%s %s\n", __func__, cool_dev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thz_dev, table_val, cool_dev)) {
		wmt_tm_info("%s error unbinding cooling dev\n", __func__);
		return -EINVAL;
	}

	wmt_tm_dprintk("%s unbinding OK\n", __func__);
	return 0;
}

static int wmt_thz_get_temp(struct thermal_zone_device *thz_dev, int *pv)
{
	/* struct wmt_thermal_ctrl_ops *p_des; */
	int temp = 0;
	int i;
	int temp_ts[NR_TS_SENSORS];
	/* int temp_ts4 = 0; */
	/* int temp_abb = 0, temp_cpu = 0, temp_gpu = 0, temp_soc = 0; */

	for (i = 0; i < NR_TS_SENSORS; i++)
		temp_ts[i] = (*ts_get_temp_wrap[i]) ();
	/* FIXME: temp_ts[0] (mtk_wcn_cmb_stub_query_ctrl) uses unit of degree,
	 * while others temp_ts[x] use milli degree
	 */
	temp_ts[0] *= 1000;
	temp = temp_ts[0];

	wmt_tm_dprintk("%s: TS1 %d TS2 %d TS3 %d\n", __func__,
					temp_ts[1], temp_ts[2], temp_ts[3]);

	wmt_tm_dprintk("%s: WIFI temp %d\n", __func__, temp);

	g_prev_temp = g_curr_temp;
	if (sensor_select < 0 || sensor_select >= NR_TS_SENSORS) {
		#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
					"%s ", __func__);
		#endif
		sensor_select = 0;
	}

	g_curr_temp = temp_ts[sensor_select];

	if (temp >= 255000)	/* dummy values */
		temp = -127000;

	*pv = temp; /* TODO: fix this. */

	if (temp != -127000) {
		if (temp > 100000 || temp < -30000)
			wmt_tm_info("[%s] temp = %d\n", __func__, temp);
	}

	if ((int)*pv >= polling_trip_temp1)
		thz_dev->polling_delay = g_wmt_tm.linux_if.interval;
	else if ((int)*pv < polling_trip_temp2)
		thz_dev->polling_delay = g_wmt_tm.linux_if.interval
							* polling_factor2;
	else
		thz_dev->polling_delay = g_wmt_tm.linux_if.interval
							* polling_factor1;

	return 0;
}

static int wmt_thz_get_mode(
struct thermal_zone_device *thz_dev, enum thermal_device_mode *mode)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;
/* int    kernel_mode = 0; */

	wmt_tm_dprintk("[%s]\n", __func__);

	if (pg_wmt_tm) {
		p_linux_if = &pg_wmt_tm->linux_if;
	} else {
		wmt_tm_dprintk("[%s] fail!\n", __func__);
		return -EINVAL;
	}

	wmt_tm_dprintk("[%s] %d\n", __func__, p_linux_if->kernel_mode);

	*mode = (p_linux_if->kernel_mode) ?
			THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;

	return 0;
}

static int wmt_thz_set_mode(
struct thermal_zone_device *thz_dev, enum thermal_device_mode mode)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;

	wmt_tm_dprintk("[%s]\n", __func__);

	if (pg_wmt_tm) {
		p_linux_if = &pg_wmt_tm->linux_if;
	} else {
		wmt_tm_dprintk("[%s] fail!\n", __func__);
		return -EINVAL;
	}

	wmt_tm_dprintk("[%s] %d\n", __func__, mode);

	p_linux_if->kernel_mode = mode;

	return 0;

}

static int wmt_thz_get_trip_type(
struct thermal_zone_device *thz_dev, int trip, enum thermal_trip_type *type)
{
	wmt_tm_dprintk("[mtktspa_get_trip_type] %d\n", trip);
	*type = g_thermal_trip[trip];
	return 0;
}

static int wmt_thz_get_trip_temp(
struct thermal_zone_device *thz_dev, int trip, int *pv)
{
	wmt_tm_dprintk("[mtktspa_get_trip_temp] %d\n", trip);
	*pv = g_trip_temp[trip];
	return 0;
}

static int wmt_thz_get_crit_temp(
struct thermal_zone_device *thz_dev, int *pv)
{
	wmt_tm_dprintk("[%s]\n", __func__);
#define WMT_TM_TEMP_CRIT (85000) /* 85.000 degree Celsius */
	*pv = WMT_TM_TEMP_CRIT;

	return 0;
}

/* +mtktspa_cooling_sysrst_ops+ */
static int wmt_cl_get_max_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = 1;
	wmt_tm_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int wmt_cl_get_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = cl_dev_state;
	wmt_tm_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int wmt_cl_set_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long v)
{
	wmt_tm_dprintk("[%s] %lu\n", __func__, v);
	cl_dev_state = v;

	if (cl_dev_state == 1) {
		wmt_tm_printk("%s = 1\n", __func__);
		/* the temperature is over than the critical, system reboot. */
		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();
	}

	return 0;
}

#define UNK_STAT (-1)
#define LOW_STAT (0)
#define MID_STAT (1)
#define HIGH_STAT (2)
#define WFD_STAT (3)

static inline unsigned long thro(
unsigned long a, unsigned int b, unsigned int c)
{
	unsigned long tmp;

	tmp = (a << 10) * b / c;

	return tmp >> 10;
}

static int wmt_judge_throttling(int index, int is_on, int interval)
{
	/*
	 *     throttling_stat
	 *        2 ( pa1=1,pa2=1 )
	 * UPPER ----
	 *        1 ( pa1=1,pa2=0 )
	 * LOWER ----
	 *        0 ( pa1=0,pa2=0 )
	 */
	static unsigned int throttling_pre_stat;
	static int mail_box[2] = { -1, -1 };

	static bool is_reset;

	unsigned long cur_thro = tx_throughput;
	static unsigned long thro_constraint = 99 * 1000;

	int cur_wifi_stat = 0;

	wmt_tm_dprintk("[%s]+ [0]=%d, [1]=%d || [%d] is %s\n", __func__,
					mail_box[0], mail_box[1],
					index, (is_on == 1 ? "ON" : "OFF"));
	mail_box[index] = is_on;

	if (mail_box[0] < 0 || mail_box[1] < 0) {
		wmt_tm_dprintk("[%s] dont get all info!!\n", __func__);
		return 0;
	}

	cur_wifi_stat = mail_box[0] + mail_box[1];

	/*
	 * If Wifi-display is on, go to WFD_STAT state,
	 *	and reset the throttling.
	 */
	if (tm_wfd_stat == 2)
		cur_wifi_stat = WFD_STAT;

	switch (cur_wifi_stat) {
	case WFD_STAT:
		if (throttling_pre_stat != WFD_STAT) {
			/*
			 * Enter Wifi-Display status, reset all throttling.
			 * Dont affect the performance of Wifi-Display.
			 */
			wmt_send_signal(-1);
			below_low_time = 0;
			over_up_time = 0;
			throttling_pre_stat = WFD_STAT;
			wmt_tm_printk("WFD is on, reset everything!");
		}
		break;

	case HIGH_STAT:
		if (throttling_pre_stat < HIGH_STAT
		|| throttling_pre_stat == WFD_STAT) {
			if (cur_thro > 0)	/*Wifi is working!! */
				thro_constraint = thro(cur_thro, up_numerator,
								up_denominator);
			else	/*At this moment, current throughput is none.
				 * Use the previous constraint.
				 */
				thro_constraint =
				    thro(thro_constraint, up_numerator,
								up_denominator);

			wmt_tm_printk("LOW/MID-->HIGH:%lu <- (%d / %d) %lu",
				      thro_constraint, up_numerator,
						up_denominator, cur_thro);

			/* wmt_send_signal( thro_constraint / 1000); */
			/* [star] unit : Kbytes */
			wmt_send_signal(thro_constraint);
			throttling_pre_stat = HIGH_STAT;
			over_up_time = 0;
		} else if (throttling_pre_stat == HIGH_STAT) {
			over_up_time++;
			if ((over_up_time * interval) >= up_duration) {
				/*real throughput may have huge variant */
				if (cur_thro < thro_constraint)
					thro_constraint =
					    thro(cur_thro, up_numerator,
								up_denominator);
				else
				/* current throughput is large than
				 * constraint. WHAT!!!
				 */
					thro_constraint =
					    thro(thro_constraint, up_numerator,
								up_denominator);

				wmt_tm_printk("HIGH-->HIGH:%lu <- (%d / %d) %lu",
						thro_constraint, up_numerator,
						up_denominator, cur_thro);

				/* wmt_send_signal( thro_constraint / 1000); */
				/* [star] unit : Kbytes */
				wmt_send_signal(thro_constraint);
				over_up_time = 0;
			}
		} else {
			wmt_tm_info("[%s] Error state1=%d!!\n", __func__,
							throttling_pre_stat);
		}
		wmt_tm_printk("case2 time=%d\n", over_up_time);
		break;

	case MID_STAT:
		if (throttling_pre_stat == LOW_STAT) {
			below_low_time = 0;
			throttling_pre_stat = MID_STAT;
			wmt_tm_printk("[%s] Go up!!\n", __func__);
		} else if (throttling_pre_stat == HIGH_STAT) {
			over_up_time = 0;
			throttling_pre_stat = MID_STAT;
			wmt_tm_printk("[%s] Go down!!\n", __func__);
		} else {
			throttling_pre_stat = MID_STAT;
			wmt_tm_dprintk("[%s] pre_stat=%d!!\n", __func__,
							throttling_pre_stat);
		}
		break;

	case LOW_STAT:
		if (throttling_pre_stat == WFD_STAT) {
			throttling_pre_stat = LOW_STAT;
			wmt_tm_dprintk("[%s] pre_stat=%d!!\n", __func__,
							throttling_pre_stat);
		} else if (throttling_pre_stat > LOW_STAT) {
			if (cur_thro < 5000 && cur_thro > 0) {
				thro_constraint = cur_thro * 3;
			} else if (cur_thro >= 5000) {
				thro_constraint = thro(cur_thro, low_numerator,
							low_denominator);
			} else {
				thro_constraint =
				    thro(thro_constraint, low_numerator,
							low_denominator);
			}

			wmt_tm_printk("MID/HIGH-->LOW:%lu <- (%d / %d) %lu",
						thro_constraint, low_numerator,
						low_denominator, cur_thro);

			/* wmt_send_signal( thro_constraint / 1000); */
			/* [star] unit : Kbytes */
			wmt_send_signal(thro_constraint);
			throttling_pre_stat = LOW_STAT;
			below_low_time = 0;
			low_rst_time = 0;
			is_reset = false;
		} else if (throttling_pre_stat == LOW_STAT) {
			below_low_time++;
			if ((below_low_time * interval) >= low_duration) {
				if (low_rst_time >= low_rst_max && !is_reset) {
					wmt_tm_printk("over rst time=%d",
								low_rst_time);

					wmt_send_signal(-1);	/* reset */
					low_rst_time = low_rst_max;
					is_reset = true;
				} else if (!is_reset) {
					if (cur_thro < 5000 && cur_thro > 0) {
						thro_constraint = cur_thro * 3;
					} else if (cur_thro >= 5000) {
						thro_constraint =
						    thro(cur_thro,
							low_numerator,
							low_denominator);

						low_rst_time++;
					} else {
						thro_constraint =
						    thro(thro_constraint,
							low_numerator,
							low_denominator);

						low_rst_time++;
					}

					wmt_tm_printk(
						"LOW-->LOW:%lu <-(%d / %d) %lu",
						thro_constraint, low_numerator,
						low_denominator, cur_thro);

					/* wmt_send_signal(
					 * thro_constraint / 1000);
					 */
					/* [star] unit : Kbytes */
					wmt_send_signal(thro_constraint);
					below_low_time = 0;
				} else {
					wmt_tm_dprintk(
						"Have reset, no control!!");
				}
			}
		} else {
			wmt_tm_info("[%s] Error state3 %d!!\n", __func__,
							throttling_pre_stat);
		}
		wmt_tm_dprintk("case0 time=%d, rst=%d %d\n", below_low_time,
							low_rst_time, is_reset);
		break;

	default:
		wmt_tm_info("[%s] Error cur_wifi_stat=%d!!\n", __func__,
								cur_wifi_stat);
		break;
	}

	mail_box[0] = UNK_STAT;
	mail_box[1] = UNK_STAT;
	return 0;
}

/* +mtktspa_cooling_pa1_ops+ */
static int wmt_cl_pa1_get_max_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = 1;
	wmt_tm_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int wmt_cl_pa1_get_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = cl_pa1_dev_state;
	wmt_tm_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int wmt_cl_pa1_set_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long v)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;
	int ret = 0;

	wmt_tm_dprintk("[%s] %d\n", __func__, __LINE__);

	cl_pa1_dev_state = (unsigned int)v;

	wmt_tm_dprintk("[%s] wifi_throttle_version=%u,cl_pa1_dev_state=%u\n",
				__func__,
				wifi_throttle_version, cl_pa1_dev_state);

	if (wifi_throttle_version == 0) {
		wmt_tm_dprintk("[%s] %lu\n", __func__, v);

		if (pg_wmt_tm) {
			p_linux_if = &pg_wmt_tm->linux_if;
			if (p_linux_if == NULL)
				return -1;
		} else {
			return -1;
		}
/* cl_pa1_dev_state = (unsigned int)v; */

		if (cl_pa1_dev_state == 1)
			ret = wmt_judge_throttling(0, 1,
						p_linux_if->interval / 1000);
		else
			ret = wmt_judge_throttling(0, 0,
						p_linux_if->interval / 1000);

		if (ret != 0)
			wmt_tm_info("[%s] ret=%d\n", __func__, ret);
	} else {
/* cl_pa1_dev_state = (unsigned int)v; */

		wmt_tm_dprintk("[%s] cl_pa1_dev_state=%u,wif curr T=%d\n",
						__func__,
						cl_pa1_dev_state, g_curr_temp);

		/* need to do limit and unlimit */
		heterogeneous_resource_allocator(g_curr_temp);
	}

	return ret;
}
/* -mtktspa_cooling_pa1_ops- */

/* +mtktspa_cooling_pa2_ops+ */
static int wmt_cl_pa2_get_max_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = 1;
	wmt_tm_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int wmt_cl_pa2_get_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long *pv)
{
	*pv = cl_pa2_dev_state;
	wmt_tm_dprintk("[%s] %lu\n", __func__, *pv);
	return 0;
}

static int wmt_cl_pa2_set_cur_state(
struct thermal_cooling_device *cool_dev, unsigned long v)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;
	int ret = 0;

	if (wifi_throttle_version == 0) {
		wmt_tm_dprintk("[%s] %lu\n", __func__, v);

		if (pg_wmt_tm) {
			p_linux_if = &pg_wmt_tm->linux_if;
			if (p_linux_if == NULL)
				return -1;
		} else {
			return -1;
		}

		cl_pa2_dev_state = (unsigned int)v;

		if (cl_pa2_dev_state == 1)
			ret = wmt_judge_throttling(1, 1,
						p_linux_if->interval / 1000);
		else
			ret = wmt_judge_throttling(1, 0,
						p_linux_if->interval / 1000);

		if (ret != 0)
			wmt_tm_info("[%s] ret=%d\n", __func__, ret);
	}

	return ret;
}
/* -mtktspa_cooling_pa2_ops- */

int wmt_wifi_tx_thro_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%lu\n", tx_throughput);

	wmt_tm_dprintk("[%s] tx=%lu\n", __func__, tx_throughput);

	return 0;
}

static int wmt_wifi_tx_thro_open(struct inode *inode, struct file *file)
{
	return single_open(file, wmt_wifi_tx_thro_read, PDE_DATA(inode));
}


int wmt_wifi_tx_thro_limit_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_limit_tput);

	wmt_tm_dprintk("[%s] tx=%d\n", __func__, g_limit_tput);

	return 0;
}

static int wmt_wifi_tx_thro_limit_open(struct inode *inode, struct file *file)
{
	return single_open(file, wmt_wifi_tx_thro_limit_read, PDE_DATA(inode));
}

int wmt_test_thro;
/* New Wifi throttling Algo+ */
ssize_t wmt_wifi_algo_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	char desc[MAX_LEN] = { 0 };

	unsigned int tmp_up_dur = 30;
	unsigned int tmp_up_den = 2;
	unsigned int tmp_up_num = 1;

	unsigned int tmp_low_dur = 3;
	unsigned int tmp_low_den = 2;
	unsigned int tmp_low_num = 3;

	unsigned int tmp_low_rst_max = 3;

	unsigned int tmp_log = 0;

	len = (len < (sizeof(desc) - 1)) ? len : (sizeof(desc) - 1);

	/* write data to the buffer */
	if (copy_from_user(desc, buf, len))
		return -EFAULT;

	if (sscanf(desc, "%d %d/%d %d %d/%d %d",
		&tmp_up_dur, &tmp_up_num, &tmp_up_den, &tmp_low_dur,
		&tmp_low_num, &tmp_low_den, &tmp_low_rst_max) == 7) {

		up_duration = tmp_up_dur;
		up_denominator = tmp_up_den;
		up_numerator = tmp_up_num;

		low_duration = tmp_low_dur;
		low_denominator = tmp_low_den;
		low_numerator = tmp_low_num;

		low_rst_max = tmp_low_rst_max;

		over_up_time = 0;
		below_low_time = 0;
		low_rst_time = 0;

		wmt_tm_printk("[%s] %s [up]%d %d/%d, [low]%d %d/%d, rst=%d\n",
				__func__, desc,
				up_duration, up_numerator, up_denominator,
				low_duration,
				low_numerator, low_denominator, low_rst_max);

		return len;
	}

	if (sscanf(desc, "log=%d", &tmp_log) == 1) {
		if (tmp_log == 1)
			wmt_tm_debug_log = 1;
		else
			wmt_tm_debug_log = 0;

		return len;
	}

	if (sscanf(desc, "thro=%d", &wmt_test_thro) == 1) {
		if (wmt_test_thro ==  0)
			wmt_test_thro = -1;
		wmt_tm_printk("set wmt thro = %d\n", wmt_test_thro);
		wmt_send_signal(wmt_test_thro);
		return len;
	}

	wmt_tm_printk("[%s] bad argument = %s\n", __func__, desc);
	return -EINVAL;
}

int wmt_wifi_algo_read(struct seq_file *m, void *v)
{
	/* int ret; */
	/* char tmp[MAX_LEN] = {0}; */

	seq_printf(m,
		"[up]\t%3d(sec)\t%2d/%2d\n[low]\t%3d(sec)\t%2d/%2d\nrst=%2d\n",
		up_duration, up_numerator, up_denominator, low_duration,
		low_numerator, low_denominator, low_rst_max);
	/* ret = strlen(tmp); */

	/* memcpy(buf, tmp, ret*sizeof(char)); */

	wmt_tm_printk("[%s] [up]%d %d/%d, [low]%d %d/%d, rst=%d\n", __func__,
		up_duration, up_numerator, up_denominator, low_duration,
		low_numerator, low_denominator, low_rst_max);

	return 0;
}

static int wmt_wifi_algo_open(struct inode *inode, struct file *file)
{
	return single_open(file, wmt_wifi_algo_read, PDE_DATA(inode));
}
/* New Wifi throttling Algo- */

ssize_t wmt_tm_wfd_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };

	len = (len < (MAX_LEN - 1)) ? len : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtoint(tmp, 10, &tm_wfd_stat);

#if 0
	wmt_tm_printk("[%s] %s = %d, len=%d, ret=%d\n"
		, __func__, tmp, tm_wfd_stat, len, ret);
#endif

	return len;
}

int wmt_tm_wfd_read(struct seq_file *m, void *v)
{
	/* int len; */
	/* int ret = 0; */
	/* char tmp[MAX_LEN] = {0}; */

	seq_printf(m, "%d\n", tm_wfd_stat);
	/* len = strlen(tmp); */

	/* memcpy(buf, tmp, ret*sizeof(char)); */

	wmt_tm_printk("[%s] %d\n", __func__, tm_wfd_stat);

	return 0;
}

static int wmt_tm_wfd_open(struct inode *inode, struct file *file)
{
	return single_open(file, wmt_tm_wfd_read, PDE_DATA(inode));
}

ssize_t wmt_wifi_in_soc_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };

	len = (len < (MAX_LEN - 1)) ? len : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = sscanf(tmp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			&wifi_throttle_version,
			&sensor_select,
			&resource_allocator_policy,
			&cpu_wifi_target_tj_offset,
			&wmt_wifi_target_tj,
			&wmt_wifi_target_offset,
			&tj_stable_range,
			&min_wifi_tput_ratio,
			&max_wifi_tput_ratio,
			&min_wifi_tput, &tt_wifi_high, &tt_wifi_low,
			&tp_wifi_rise, &tp_wifi_fall);

	wmt_tm_printk(
		"qqq %s ret :%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		__func__, ret,
		wifi_throttle_version,
		sensor_select,
		resource_allocator_policy,
		cpu_wifi_target_tj_offset,
		wmt_wifi_target_tj,
		wmt_wifi_target_offset,
		tj_stable_range,
		min_wifi_tput_ratio,
		max_wifi_tput_ratio,
		min_wifi_tput, tt_wifi_high,
		tt_wifi_low, tp_wifi_rise, tp_wifi_fall);

	if (ret != 14 || sensor_select < 0 || sensor_select >= NR_TS_SENSORS) {
		#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
					"%s ",	__func__);
		#endif
		sensor_select = 0;
	}
/* wmt_wifi_in_soc_write ret 13 1 3 3 0 70000 3000 1000
 * 50 200 1000 50 50 10000
 */
	return len;
}

int wmt_wifi_in_soc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		wifi_throttle_version,
		sensor_select,
		resource_allocator_policy,
		cpu_wifi_target_tj_offset,
		wmt_wifi_target_tj,
		wmt_wifi_target_offset,
		tj_stable_range,
		min_wifi_tput_ratio,
		max_wifi_tput_ratio,
		min_wifi_tput, tt_wifi_high, tt_wifi_low,
		tp_wifi_rise, tp_wifi_fall);

	/* wmt_tm_printk("[%s] %d\n", __func__, wifi_in_soc); */

	return 0;
}

static int wmt_wifi_in_soc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wmt_wifi_in_soc_read, PDE_DATA(inode));
}

ssize_t wmt_tm_pid_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };

	len = (len < (MAX_LEN - 1)) ? len : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &tm_input_pid);
	if (ret)
		WARN_ON_ONCE(1);

	wmt_tm_printk("[%s] %s = %d\n", __func__, tmp, tm_input_pid);

	return len;
}

int wmt_tm_pid_read(struct seq_file *m, void *v)
{
	/* int ret; */
	/* char tmp[MAX_LEN] = {0}; */

	seq_printf(m, "%d\n", tm_input_pid);
	/* ret = strlen(tmp); */

	/* memcpy(buf, tmp, ret*sizeof(char)); */

	wmt_tm_printk("[%s] %d\n", __func__, tm_input_pid);

	return 0;
}

static int wmt_tm_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, wmt_tm_pid_read, PDE_DATA(inode));
}

#define check_str(x) (x[0] == '\0'?"none\t":x)

static void mtkts_wmt_cancel_thermal_timer(void)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;

	/* wmt_tm_dprintk("[%s]\n", __func__); */

	if (pg_wmt_tm)
		p_linux_if = &pg_wmt_tm->linux_if;
	else
		return;

	/* pr_debug("mtkts_wmt_cancel_thermal_timer\n"); */

	/* stop thermal framework polling when entering deep idle */

	if (down_trylock(&sem_mutex))
		return;

	if (p_linux_if->thz_dev) {
		cancel_delayed_work(&(p_linux_if->thz_dev->poll_queue));
		isTimerCancelled = 1;
	}

	up(&sem_mutex);
}

static void mtkts_wmt_start_thermal_timer(void)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;

	/* wmt_tm_dprintk("[%s]\n", __func__); */

	if (pg_wmt_tm)
		p_linux_if = &pg_wmt_tm->linux_if;
	else
		return;

	/* pr_debug("mtkts_wmt_start_thermal_timer\n"); */
	/* resume thermal framework polling when leaving deep idle */

	if (!isTimerCancelled)
		return;


	if (down_trylock(&sem_mutex))
		return;

	if (p_linux_if->thz_dev != NULL && p_linux_if->interval != 0) {
		mod_delayed_work(system_freezable_power_efficient_wq,
					&(p_linux_if->thz_dev->poll_queue),
					round_jiffies(msecs_to_jiffies(2000)));
		isTimerCancelled = 0;
	}
	up(&sem_mutex);
}

static struct thermal_zone_device_ops wmt_thz_dev_ops = {
	.bind = wmt_thz_bind,
	.unbind = wmt_thz_unbind,
	.get_temp = wmt_thz_get_temp,
	.get_mode = wmt_thz_get_mode,
	.set_mode = wmt_thz_set_mode,
	.get_trip_type = wmt_thz_get_trip_type,
	.get_trip_temp = wmt_thz_get_trip_temp,
	.get_crit_temp = wmt_thz_get_crit_temp,
};

static struct thermal_cooling_device_ops mtktspa_cooling_sysrst_ops = {
	.get_max_state = wmt_cl_get_max_state,
	.get_cur_state = wmt_cl_get_cur_state,
	.set_cur_state = wmt_cl_set_cur_state,
};

static struct thermal_cooling_device_ops mtktspa_cooling_pa1_ops = {
	.get_max_state = wmt_cl_pa1_get_max_state,
	.get_cur_state = wmt_cl_pa1_get_cur_state,
	.set_cur_state = wmt_cl_pa1_set_cur_state,
};

static struct thermal_cooling_device_ops mtktspa_cooling_pa2_ops = {
	.get_max_state = wmt_cl_pa2_get_max_state,
	.get_cur_state = wmt_cl_pa2_get_cur_state,
	.set_cur_state = wmt_cl_pa2_set_cur_state,
};

static int wmt_tm_thz_cl_register(void)
{
	/*Default disable, turn on by thermal policy */
	#define DEFAULT_POLL_TIME 0

	struct linux_thermal_ctrl_if *p_linux_if = 0;

	wmt_tm_dprintk("[%s]\n", __func__);

	if (pg_wmt_tm)
		p_linux_if = &pg_wmt_tm->linux_if;
	else
		return -1;

	/* cooling devices */
	p_linux_if->cl_dev =
		mtk_thermal_cooling_device_register("mtktswmt-sysrst", NULL,
						&mtktspa_cooling_sysrst_ops);

	p_linux_if->cl_pa1_dev =
		mtk_thermal_cooling_device_register("mtktswmt-pa1", NULL,
						&mtktspa_cooling_pa1_ops);

	p_linux_if->cl_pa2_dev =
		mtk_thermal_cooling_device_register("mtktswmt-pa2", NULL,
						&mtktspa_cooling_pa2_ops);

	p_linux_if->interval = DEFAULT_POLL_TIME;

	/* trips */
	p_linux_if->thz_dev =
		mtk_thermal_zone_device_register("mtktswmt", g_num_trip, NULL,
						&wmt_thz_dev_ops, 0, 0, 0,
						p_linux_if->interval);

	return 0;
}

static int wmt_tm_read(struct seq_file *m, void *v)
{
	/* int len = 0; */
	/* char *p = buf; */
	struct linux_thermal_ctrl_if *p_linux_if = 0;

	wmt_tm_printk("[%s]\n", __func__);

	/* sanity */
	if (pg_wmt_tm) {
		p_linux_if = &pg_wmt_tm->linux_if;
	} else {
		wmt_tm_info("[%s] fail!\n", __func__);
		return -EINVAL;
	}

	seq_printf(m,
		"[%s]\n \tcooler\t\ttrip_temp\ttrip_type\n [0] %s\t%d\t\t%d\n [1] %s\t%d\t\t%d",
		__func__,
		check_str(g_bind0), g_trip_temp[0], g_thermal_trip[0],
		check_str(g_bind1), g_trip_temp[1], g_thermal_trip[1]);

	seq_printf(m,
		"\n [2] %s\t%d\t\t%d\n [3] %s\t%d\t\t%d\n [4] %s\t%d\t\t%d\n [5] %s\t%d\t\t%d",
		check_str(g_bind2), g_trip_temp[2], g_thermal_trip[2],
		check_str(g_bind3), g_trip_temp[3], g_thermal_trip[3],
		check_str(g_bind4), g_trip_temp[4], g_thermal_trip[4],
		check_str(g_bind5), g_trip_temp[5], g_thermal_trip[5]);

	seq_printf(m,
		"\n [6] %s\t%d\t\t%d\n [7] %s\t%d\t\t%d\n [8] %s\t%d\t\t%d\n [9] %s\t%d\t\t%d\ntime_ms=%d\n",
		check_str(g_bind6), g_trip_temp[6], g_thermal_trip[6],
		check_str(g_bind7), g_trip_temp[7], g_thermal_trip[7],
		check_str(g_bind8), g_trip_temp[8], g_thermal_trip[8],
		check_str(g_bind9), g_trip_temp[9], g_thermal_trip[9],
		p_linux_if->interval);

	return 0;
}

static int wmt_tm_open(struct inode *inode, struct file *file)
{
	return single_open(file, wmt_tm_read, PDE_DATA(inode));
}

static ssize_t wmt_tm_write(
struct file *filp, const char __user *buf, size_t count, loff_t *data)
{
	int i = 0;
	int len = 0;
	struct tm_data {
		int trip_temp[COOLER_NUM];
		int thermal_trip[COOLER_NUM];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct linux_thermal_ctrl_if *p_linux_if = 0;

	struct tm_data *ptr_tm_data = kmalloc(sizeof(*ptr_tm_data), GFP_KERNEL);

	if (ptr_tm_data == NULL) {
		/* wmt_tm_printk("[%s] kmalloc fail\n\n", __func__); */
		return -ENOMEM;
	}

	wmt_tm_printk("[%s]\n", __func__);

	/* sanity */
	if (pg_wmt_tm) {
		p_linux_if = &pg_wmt_tm->linux_if;
	} else {
		wmt_tm_info("[wmt_thz_write] fail!\n");
		kfree(ptr_tm_data);
		return -EINVAL;
	}

	len = (count < (sizeof(ptr_tm_data->desc) - 1)) ?
				count : (sizeof(ptr_tm_data->desc) - 1);

	if (copy_from_user(ptr_tm_data->desc, buf, len)) {
		kfree(ptr_tm_data);
		return 0;
	}

	ptr_tm_data->desc[len] = '\0';

	if (sscanf(ptr_tm_data->desc,
		"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&g_num_trip,
		&ptr_tm_data->trip_temp[0], &ptr_tm_data->thermal_trip[0],
		ptr_tm_data->bind0,
		&ptr_tm_data->trip_temp[1], &ptr_tm_data->thermal_trip[1],
		ptr_tm_data->bind1,
		&ptr_tm_data->trip_temp[2], &ptr_tm_data->thermal_trip[2],
		ptr_tm_data->bind2,
		&ptr_tm_data->trip_temp[3], &ptr_tm_data->thermal_trip[3],
		ptr_tm_data->bind3,
		&ptr_tm_data->trip_temp[4], &ptr_tm_data->thermal_trip[4],
		ptr_tm_data->bind4,
		&ptr_tm_data->trip_temp[5], &ptr_tm_data->thermal_trip[5],
		ptr_tm_data->bind5,
		&ptr_tm_data->trip_temp[6], &ptr_tm_data->thermal_trip[6],
		ptr_tm_data->bind6,
		&ptr_tm_data->trip_temp[7], &ptr_tm_data->thermal_trip[7],
		ptr_tm_data->bind7,
		&ptr_tm_data->trip_temp[8], &ptr_tm_data->thermal_trip[8],
		ptr_tm_data->bind8,
		&ptr_tm_data->trip_temp[9], &ptr_tm_data->thermal_trip[9],
		ptr_tm_data->bind9,
		&ptr_tm_data->time_msec) == 32) {

		/* unregister */
		down(&sem_mutex);
		wmt_tm_dprintk("[%s] mtktswmt unregister thermal\n", __func__);
		if (p_linux_if->thz_dev) {
			mtk_thermal_zone_device_unregister(p_linux_if->thz_dev);
			p_linux_if->thz_dev = NULL;
		}

		if (g_num_trip < 0 || g_num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT, "wmt_tm_write",
						"Bad argument");
			#endif
			wmt_tm_info("[%s] bad argument = %s\n", __func__,
						ptr_tm_data->desc);
			kfree(ptr_tm_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < g_num_trip; i++)
			g_thermal_trip[i] = ptr_tm_data->thermal_trip[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
							= g_bind4[0] = '\0';

		g_bind5[0] = g_bind6[0] = g_bind7[0] = g_bind8[0]
							= g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_tm_data->bind0[i];
			g_bind1[i] = ptr_tm_data->bind1[i];
			g_bind2[i] = ptr_tm_data->bind2[i];
			g_bind3[i] = ptr_tm_data->bind3[i];
			g_bind4[i] = ptr_tm_data->bind4[i];
			g_bind5[i] = ptr_tm_data->bind5[i];
			g_bind6[i] = ptr_tm_data->bind6[i];
			g_bind7[i] = ptr_tm_data->bind7[i];
			g_bind8[i] = ptr_tm_data->bind8[i];
			g_bind9[i] = ptr_tm_data->bind9[i];
		}

		for (i = 0; i < g_num_trip; i++)
			g_trip_temp[i] = ptr_tm_data->trip_temp[i];

		p_linux_if->interval = ptr_tm_data->time_msec;

		wmt_tm_dprintk(
			"[%s] g_trip_temp [0]=%d, [1]=%d, [2]=%d, [3]=%d, [4]=%d\n",
			__func__,
			g_thermal_trip[0], g_thermal_trip[1], g_thermal_trip[2],
			g_thermal_trip[3], g_thermal_trip[4]);

		wmt_tm_dprintk(
			"[%s] g_trip_temp [5]=%d, [6]=%d, [7]=%d, [8]=%d, [9]=%d\n",
			__func__,
			g_thermal_trip[5], g_thermal_trip[6], g_thermal_trip[7],
			g_thermal_trip[8], g_thermal_trip[9]);

		wmt_tm_dprintk(
			"[%s] cooldev [0]=%s, [1]=%s, [2]=%s, [3]=%s, [4]=%s,\n",
			__func__,
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		wmt_tm_dprintk(
			"[%s] cooldev [5]=%s, [6]=%s, [7]=%s, [8]=%s, [9]=%s,\n",
			__func__,
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		wmt_tm_dprintk(
			"[%s] trip_temp [0]=%d, [1]=%d, [2]=%d, [3]=%d, [4]=%d\n",
			__func__,
			ptr_tm_data->trip_temp[0], ptr_tm_data->trip_temp[1],
			ptr_tm_data->trip_temp[2], ptr_tm_data->trip_temp[3],
			ptr_tm_data->trip_temp[4]);

		wmt_tm_dprintk(
			"[%s] trip_temp [5]=%d, [6]=%d, [7]=%d, [8]=%d, [9]=%d\n",
			__func__,
			ptr_tm_data->trip_temp[5], ptr_tm_data->trip_temp[6],
			ptr_tm_data->trip_temp[7], ptr_tm_data->trip_temp[8],
			ptr_tm_data->trip_temp[9]);

		wmt_tm_dprintk("[%s] polling time=%d\n", __func__,
							p_linux_if->interval);

		/* p_linux_if->thz_dev->polling_delay
		 *			= p_linux_if->interval*1000;
		 */

		/* thermal_zone_device_update(p_linux_if->thz_dev); */

		/* register */
		wmt_tm_dprintk("[%s] mtktswmt register thermal\n", __func__);
		p_linux_if->thz_dev =
			mtk_thermal_zone_device_register("mtktswmt",
						g_num_trip, NULL,
						&wmt_thz_dev_ops, 0, 0, 0,
						p_linux_if->interval);
		up(&sem_mutex);

		wmt_tm_dprintk("[%s] time_ms=%d\n", __func__,
						p_linux_if->interval);

		kfree(ptr_tm_data);

		return count;
	}

	wmt_tm_info("[%s] bad argument = %s\n", __func__, ptr_tm_data->desc);
    #ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
							"wmt_tm_write",
							"Bad argument");
    #endif
	kfree(ptr_tm_data);
	return -EINVAL;
}

static const struct file_operations _wmt_tm_fops = {
	.owner = THIS_MODULE,
	.open = wmt_tm_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = wmt_tm_write,
	.release = single_release,
};

static const struct file_operations _tm_pid_fops = {
	.owner = THIS_MODULE,
	.open = wmt_tm_pid_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = wmt_tm_pid_write,
	.release = single_release,
};

static const struct file_operations _wmt_val_fops = {
	.owner = THIS_MODULE,
	.open = wmt_wifi_algo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = wmt_wifi_algo_write,
	.release = single_release,
};

static const struct file_operations _tx_thro_fops = {
	.owner = THIS_MODULE,
	.open = wmt_wifi_tx_thro_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations _tx_thro_limit_fops = {
	.owner = THIS_MODULE,
	.open = wmt_wifi_tx_thro_limit_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations _wfd_stat_fops = {
	.owner = THIS_MODULE,
	.open = wmt_tm_wfd_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = wmt_tm_wfd_write,
	.release = single_release,
};

static const struct file_operations _wifi_in_soc_fops = {
	.owner = THIS_MODULE,
	.open = wmt_wifi_in_soc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = wmt_wifi_in_soc_write,
	.release = single_release,
};

static int wmt_tm_proc_register(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *wmt_tm_proc_dir = NULL;
	struct proc_dir_entry *wmt_thro_proc_dir = NULL;

	wmt_tm_dprintk("[%s]\n", __func__);

	wmt_thro_proc_dir = proc_mkdir("wmt_tm", NULL);

	if (!wmt_thro_proc_dir) {
		wmt_tm_printk(
			"[%s]: mkdir /proc/wmt_tm failed\n", __func__);
	} else {
		proc_create("tx_thro", 0644, wmt_thro_proc_dir, &_tx_thro_fops);
		proc_create("tx_thro_limit", 0644, wmt_thro_proc_dir,
							&_tx_thro_limit_fops);
	}

	wmt_tm_proc_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!wmt_tm_proc_dir) {
		wmt_tm_printk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzwmt", 0660, wmt_tm_proc_dir,
							&_wmt_tm_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("clwmtx_pid", 0660, wmt_tm_proc_dir,
								&_tm_pid_fops);

		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("clwmt_val", 0660,
					wmt_tm_proc_dir, &_wmt_val_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("clwmt_wfdstat", 0660,
					wmt_tm_proc_dir, &_wfd_stat_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("wifi_in_soc", 0660,
					wmt_tm_proc_dir, &_wifi_in_soc_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
	return 0;
}

static int wmt_tm_proc_unregister(void)
{
	wmt_tm_dprintk("[%s]\n", __func__);
	/* remove_proc_entry("wmt_tm", proc_entry); */
	return 0;
}

static int wmt_tm_thz_cl_unregister(void)
{
	struct linux_thermal_ctrl_if *p_linux_if = 0;

	wmt_tm_dprintk("[%s]\n", __func__);

	if (pg_wmt_tm)
		p_linux_if = &pg_wmt_tm->linux_if;
	else
		return -1;

	if (p_linux_if->cl_dev) {
		mtk_thermal_cooling_device_unregister(p_linux_if->cl_dev);
		p_linux_if->cl_dev = NULL;
	}

	if (p_linux_if->cl_pa1_dev) {
		mtk_thermal_cooling_device_unregister(p_linux_if->cl_pa1_dev);
		p_linux_if->cl_pa1_dev = NULL;
	}

	if (p_linux_if->cl_pa2_dev) {
		mtk_thermal_cooling_device_unregister(p_linux_if->cl_pa2_dev);
		p_linux_if->cl_pa2_dev = NULL;
	}

	if (p_linux_if->thz_dev) {
		mtk_thermal_zone_device_unregister(p_linux_if->thz_dev);
		p_linux_if->thz_dev = NULL;
	}

	return 0;
}

static int __init wmt_tm_init(void)
{
	int err = 0;

	wmt_tm_printk("[%s] start -->\n", __func__);

	err = wmt_tm_proc_register();
	if (err)
		return err;

	/* init a timer for stats tx bytes */
	wmt_stats_info.pre_time = 0;
	wmt_stats_info.pre_tx_bytes = 0;

	init_timer_deferrable(&wmt_stats_timer);
	wmt_stats_timer.function = &wmt_cal_stats;
	wmt_stats_timer.data = (unsigned long)&wmt_stats_info;
	wmt_stats_timer.expires = jiffies + 1 * HZ;
	add_timer(&wmt_stats_timer);

	err = wmt_tm_thz_cl_register();
	if (err)
		return err;

	mtkTTimer_register("mtktswmt", mtkts_wmt_start_thermal_timer,
					mtkts_wmt_cancel_thermal_timer);

	wmt_tm_printk("[%s] end <--\n", __func__);

	return 0;
}

static void __exit wmt_tm_deinit(void)
{
	int err = 0;

	wmt_tm_printk("[%s]\n", __func__);

	err = wmt_tm_thz_cl_unregister();
	if (err)
		return;

	err = wmt_tm_proc_unregister();
	if (err)
		return;

	mtkTTimer_unregister("mtktswmt");
	del_timer(&wmt_stats_timer);
}

/* EXPORT_SYMBOL(wifi_in_soc_throttle_enable); */
module_init(wmt_tm_init);
module_exit(wmt_tm_deinit);
