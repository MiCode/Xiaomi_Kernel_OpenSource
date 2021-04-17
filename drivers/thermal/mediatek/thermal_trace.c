// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/sysfs.h>
#include <linux/timer.h>
#include <linux/types.h>
#define CREATE_TRACE_POINTS
#include "thermal_trace.h"

/*==================================================
 * trace point API export
 *==================================================
 */
#if IS_ENABLED(CONFIG_MTK_MD_THERMAL)
EXPORT_TRACEPOINT_SYMBOL_GPL(md_mutt_limit);
EXPORT_TRACEPOINT_SYMBOL_GPL(md_tx_pwr_limit);
EXPORT_TRACEPOINT_SYMBOL_GPL(md_scg_off);
#endif
EXPORT_TRACEPOINT_SYMBOL_GPL(network_tput);
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
EXPORT_TRACEPOINT_SYMBOL_GPL(cpu_hr_info_0);
EXPORT_TRACEPOINT_SYMBOL_GPL(cpu_hr_info_1);
#endif
/*==================================================
 * Throughput calculator data
 *==================================================
 */
#define TIMER_INTERVAL_MS		(1000)
#define for_each_tput_instance(i)	\
	for (i = 0; i < NR_TPUT_CALC_INSTANCE; i++)

enum tput_calc_instance {
	TPUT_MD,
	TPUT_WIFI,

	NR_TPUT_CALC_INSTANCE,
};

struct tput_stats {
	unsigned long pre_tx_bytes;
	unsigned long cur_tx_bytes;
	unsigned long cur_tput;
};

struct thermal_trace {
	int enable;
	struct tput_stats stats[NR_TPUT_CALC_INSTANCE];
	struct timer_list timer;
	struct work_struct work;
	unsigned long last_update_time;
};

static struct thermal_trace thermal_trace_data;

struct headroom_trace {
	int period;
	//struct tput_stats stats[NR_TPUT_CALC_INSTANCE];
	struct timer_list timer;
	struct work_struct work;
	unsigned long last_update_time;
};

static struct headroom_trace headroom_trace_data;

/*==================================================
 * Throughput calculator local function
 *==================================================
 */
static void trace_timer_add(void)
{
	mod_timer(&thermal_trace_data.timer, jiffies +
		msecs_to_jiffies(TIMER_INTERVAL_MS));
}

static void trace_timer_del(void)
{
	del_timer(&thermal_trace_data.timer);
}

static void get_tx_bytes(struct thermal_trace *data)
{
	struct net_device *dev;
	struct net *net;
	int i;

	for_each_tput_instance(i)
		data->stats[i].cur_tx_bytes = 0;

	read_lock(&dev_base_lock);
	for_each_net(net) {
		for_each_netdev(net, dev) {
			struct rtnl_link_stats64 temp;
			struct rtnl_link_stats64 *stats;
			struct tput_stats *tput_stat;

			if (!strncmp(dev->name, "ccmni", 5)) {
				tput_stat = &data->stats[TPUT_MD];
				stats =	dev_get_stats(dev, &temp);
				tput_stat->cur_tx_bytes =
					tput_stat->cur_tx_bytes +
					stats->tx_bytes;
			} else if (!strncmp(dev->name, "wlan", 4)
				|| !strncmp(dev->name, "ap", 2)
				|| !strncmp(dev->name, "p2p", 3)) {
				tput_stat = &data->stats[TPUT_WIFI];
				stats =	dev_get_stats(dev, &temp);
				tput_stat->cur_tx_bytes =
					tput_stat->cur_tx_bytes +
					stats->tx_bytes;
			}
		}
	}
	read_unlock(&dev_base_lock);
}

static void headroom_trace_timer_add(unsigned int period)
{
	mod_timer(&headroom_trace_data.timer, jiffies +
		msecs_to_jiffies(period));
}

static void headroom_trace_timer_del(void)
{
	del_timer(&headroom_trace_data.timer);
}

static void thermal_trace_work(struct work_struct *work)
{
	struct thermal_trace *trace_data =
		container_of(work, struct thermal_trace, work);
	struct timespec64 time_spec64;
	struct timespec64 cur_time;
	long pre_time = trace_data->last_update_time;
	int i;

	ktime_get_ts64(&time_spec64);
	cur_time.tv_sec = time_spec64.tv_sec;
	cur_time.tv_nsec = time_spec64.tv_nsec;

	if (pre_time != 0 && cur_time.tv_sec > pre_time) {
		unsigned long diff;
		struct tput_stats *stat;

		get_tx_bytes(trace_data);
		for_each_tput_instance(i) {
			stat = &trace_data->stats[i];
			diff = (stat->cur_tx_bytes >= stat->pre_tx_bytes)
				? (stat->cur_tx_bytes - stat->pre_tx_bytes)
				: (0xffffffff - stat->pre_tx_bytes
				+ stat->cur_tx_bytes);
			stat->cur_tput =
				(diff / (cur_time.tv_sec - pre_time)) >> 7;

			pr_debug("[%s] %d:time/tx/tput=%lu/%lu/%luKb/s\n",
				__func__, i, cur_time.tv_sec,
				stat->cur_tx_bytes, stat->cur_tput);
			stat->pre_tx_bytes = stat->cur_tx_bytes;
		}
	}

	trace_data->last_update_time = cur_time.tv_sec;
	pr_debug("[%s] pre_time=%lu, tv_sec=%lu\n", __func__,
				pre_time, cur_time.tv_sec);

	trace_network_tput(trace_data->stats[0].cur_tput,
			trace_data->stats[1].cur_tput);

	trace_timer_add();
}

static void thermal_trace_func(struct timer_list *t)
{
	struct thermal_trace *data = from_timer(data, t, timer);

	schedule_work(&data->work);
}

static void headroom_trace_work(struct work_struct *work)
{
	struct headroom_trace *trace_data =
		container_of(work, struct headroom_trace, work);
	struct timespec64 time_spec64;
	struct timespec64 cur_time;
	unsigned long pre_time = trace_data->last_update_time;
	int i;
	struct headroom_info hr_info[NR_CPUS];
	static int prev_first_cpu_temp = 25000;
	static int prev_last_cpu_temp = 25000;

	ktime_get_ts64(&time_spec64);
	cur_time.tv_sec = time_spec64.tv_sec;
	cur_time.tv_nsec = time_spec64.tv_nsec;

	if (pre_time != 0 && cur_time.tv_sec > pre_time) {
		/* parse hr info */
		for_each_possible_cpu(i) {
			hr_info[i].temp = sign_extend32(
				readl(thermal_csram_base + CPU_TEMP_OFFSET + 4 * i), 31);
			hr_info[i].predict_temp = sign_extend32(
				readl(thermal_csram_base + CPU_PREDICT_TEMP_OFFSET + 4 * i), 31);
			hr_info[i].headroom = sign_extend32(
				readl(thermal_csram_base + CPU_HEADROOM_OFFSET + 4 * i), 31);
			hr_info[i].ratio = readl(thermal_csram_base + CPU_HEADROOM_RATIO_OFFSET + 4 * i);

			pr_debug("[%d] temp=%d, predict_temp=%d, headroom=%d, ratio=%d\n", i, hr_info[i].temp,
				hr_info[i].predict_temp,
				hr_info[i].headroom, hr_info[i].ratio);
		}
	}

		trace_data->last_update_time = cur_time.tv_sec;
		pr_debug("[%s] pre_time=%lu, tv_sec=%lu\n", __func__,
					pre_time, cur_time.tv_sec);

	if (prev_first_cpu_temp != hr_info[0].temp
		|| prev_last_cpu_temp != hr_info[NR_CPUS-1].temp) {
		trace_cpu_hr_info_0(&hr_info[0], &hr_info[1], &hr_info[2], &hr_info[3]);
		trace_cpu_hr_info_1(&hr_info[4], &hr_info[5], &hr_info[6], &hr_info[7]);
		prev_first_cpu_temp = hr_info[0].temp;
		prev_last_cpu_temp = hr_info[NR_CPUS-1].temp;
	}

	headroom_trace_timer_add(headroom_trace_data.period);
}

static void headroom_trace_func(struct timer_list *t)
{
	struct headroom_trace *data = from_timer(data, t, timer);

	schedule_work(&data->work);
}

static ssize_t enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "trace enable = %d\n",
		thermal_trace_data.enable);
	len += snprintf(buf + len, PAGE_SIZE - len,
		"MD/WIFI tput = %d/%d Kb/s\n",
		thermal_trace_data.stats[TPUT_MD].cur_tput,
		thermal_trace_data.stats[TPUT_WIFI].cur_tput);

	return len;
}

static ssize_t enable_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int enable;

	if ((kstrtouint(buf, 10, &enable)))
		return -EINVAL;

	enable = (enable > 0) ? 1 : 0;

	if (enable == thermal_trace_data.enable)
		return count;

	if (!thermal_trace_data.enable)
		trace_timer_add();
	else
		trace_timer_del();
	thermal_trace_data.enable = enable;

	return count;
}

static struct kobj_attribute thermal_trace_attr = __ATTR_RW(enable);
static struct attribute *thermal_trace_attrs[] = {
	&thermal_trace_attr.attr,
	NULL
};
static struct attribute_group thermal_trace_attr_group = {
	.name	= "thermal_trace",
	.attrs	= thermal_trace_attrs,
};


static ssize_t headroom_period_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "trace period = %d\n",
		headroom_trace_data.period);

	return len;
}

static ssize_t headroom_period_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int period;

	if ((kstrtouint(buf, 10, &period)))
		return -EINVAL;

	period = (period > 0) ? period : 0;

	if (period == headroom_trace_data.period)
		return count;

	if (period > 0)
		headroom_trace_timer_add(period);
	else
		headroom_trace_timer_del();
	headroom_trace_data.period = period;

	return count;
}

static struct kobj_attribute headroom_trace_attr = __ATTR_RW(headroom_period);
static struct attribute *headroom_trace_attrs[] = {
	&headroom_trace_attr.attr,
	NULL
};
static struct attribute_group headroom_trace_attr_group = {
	.name	= "headroom_trace",
	.attrs	= headroom_trace_attrs,
};

static int __init thermal_trace_init(void)
{
	int i, ret;

	thermal_trace_data.last_update_time = 0;
	for_each_tput_instance(i) {
		thermal_trace_data.stats[i].pre_tx_bytes = 0;
		thermal_trace_data.stats[i].cur_tput = 0;
	}

	timer_setup(&thermal_trace_data.timer, thermal_trace_func, 0);
	INIT_WORK(&thermal_trace_data.work, thermal_trace_work);

	ret = sysfs_create_group(kernel_kobj, &thermal_trace_attr_group);
	if (ret)
		pr_info("failed to create thermal_trace sysfs, ret=%d!\n", ret);

	headroom_trace_data.last_update_time = 0;

	timer_setup(&headroom_trace_data.timer, headroom_trace_func, 0);
	INIT_WORK(&headroom_trace_data.work, headroom_trace_work);

	ret = sysfs_create_group(kernel_kobj, &headroom_trace_attr_group);
	if (ret)
		pr_info("failed to create headroom_trace sysfs, ret=%d!\n", ret);

	return ret;
}
module_init(thermal_trace_init)

static void __exit thermal_trace_exit(void)
{
	trace_timer_del();
}
module_exit(thermal_trace_exit)

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek thermal trace driver");
MODULE_LICENSE("GPL v2");
