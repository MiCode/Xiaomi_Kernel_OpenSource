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
#if IS_ENABLED(CONFIG_MTK_THERMAL_JATM)
EXPORT_TRACEPOINT_SYMBOL_GPL(jatm_enable);
EXPORT_TRACEPOINT_SYMBOL_GPL(jatm_disable);
EXPORT_TRACEPOINT_SYMBOL_GPL(not_start_reason);
EXPORT_TRACEPOINT_SYMBOL_GPL(try_enable_jatm);
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

struct thermal_info {
	struct mutex lock;
	struct tput_stats stats[NR_TPUT_CALC_INSTANCE];
	struct delayed_work poll_queue;
	unsigned long last_update_time;
};

static struct thermal_info thermal_info_data;

struct thermal_trace {
	int enable;
	int hr_enable;
	unsigned long hr_period;
	struct mutex lock;
	struct hrtimer trace_timer;
};

static struct thermal_trace thermal_trace_data;

static struct thermal_cpu_info cpu_info;
static struct thermal_gpu_info gpu_info;
static struct thermal_apu_info apu_info;

/*==================================================
 * Throughput calculator local function
 *==================================================
 */
static void thermal_info_timer_add(int enable)
{
	mutex_lock(&thermal_info_data.lock);
	if (enable)
        mod_delayed_work(system_freezable_power_efficient_wq,
                 &thermal_info_data.poll_queue,
                 msecs_to_jiffies(TIMER_INTERVAL_MS));
	else
        cancel_delayed_work(&thermal_info_data.poll_queue);

	mutex_unlock(&thermal_info_data.lock);
}

static void get_tx_bytes(struct thermal_info *data)
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
				if (stats != NULL)
					tput_stat->cur_tx_bytes =
						tput_stat->cur_tx_bytes +
						stats->tx_bytes;
			} else if (!strncmp(dev->name, "wlan", 4)
				|| !strncmp(dev->name, "ap", 2)
				|| !strncmp(dev->name, "p2p", 3)) {
				tput_stat = &data->stats[TPUT_WIFI];
				stats =	dev_get_stats(dev, &temp);
				if (stats != NULL)
					tput_stat->cur_tx_bytes =
						tput_stat->cur_tx_bytes +
						stats->tx_bytes;
			}
		}
	}
	read_unlock(&dev_base_lock);
}

static void thermal_trace_timer_cancel(void)
{
	hrtimer_try_to_cancel(&thermal_trace_data.trace_timer);
}

static void thermal_trace_timer_add(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, thermal_trace_data.hr_period);
	hrtimer_start(&thermal_trace_data.trace_timer, ktime, HRTIMER_MODE_REL);
}

static void thermal_info_work(struct work_struct *work)
{
	struct thermal_info *trace_data =
		container_of(work, struct thermal_info, poll_queue.work);
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
	trace_frs(&frs_data);

	thermal_info_timer_add(thermal_trace_data.enable);
}

static void get_cpu_info()
{
	cpu_info.ttj = sign_extend32(
			readl(thermal_csram_base + CPU_TTJ_OFFSET), 31);
	cpu_info.limit_powerbudget = sign_extend32(
			readl(thermal_csram_base + CPU_POWERBUDGET_OFFSET), 31);
	cpu_info.LL_min_opp_hint = sign_extend32(
			readl(thermal_csram_base + CPU_LL_MIN_OPP_HINT_OFFSET), 31);
	cpu_info.BL_min_opp_hint = sign_extend32(
			readl(thermal_csram_base + CPU_BL_MIN_OPP_HINT_OFFSET), 31);
	cpu_info.B_min_opp_hint = sign_extend32(
			readl(thermal_csram_base + CPU_B_MIN_OPP_HINT_OFFSET), 31);
	cpu_info.LL_limit_opp = sign_extend32(
			readl(thermal_csram_base + CPU_LL_LIMIT_OPP_OFFSET), 31);
	cpu_info.BL_limit_opp = sign_extend32(
			readl(thermal_csram_base + CPU_BL_LIMIT_OPP_OFFSET), 31);
	cpu_info.B_limit_opp = sign_extend32(
			readl(thermal_csram_base + CPU_B_LIMIT_OPP_OFFSET), 31);
	cpu_info.LL_limit_freq = readl(thermal_csram_base + CPU_LL_LIMIT_FREQ_OFFSET);
	cpu_info.BL_limit_freq = readl(thermal_csram_base + CPU_BL_LIMIT_FREQ_OFFSET);
	cpu_info.B_limit_freq = readl(thermal_csram_base + CPU_B_LIMIT_FREQ_OFFSET);
	cpu_info.LL_cur_freq = readl(thermal_csram_base + CPU_LL_CUR_FREQ_OFFSET);
	cpu_info.BL_cur_freq = readl(thermal_csram_base + CPU_BL_CUR_FREQ_OFFSET);
	cpu_info.B_cur_freq = readl(thermal_csram_base + CPU_B_CUR_FREQ_OFFSET);
	cpu_info.LL_max_temp = sign_extend32(
			readl(thermal_csram_base + CPU_LL_MAX_TEMP_OFFSET), 31);
	cpu_info.BL_max_temp = sign_extend32(
			readl(thermal_csram_base + CPU_BL_MAX_TEMP_OFFSET), 31);
	cpu_info.B_max_temp = sign_extend32(
			readl(thermal_csram_base + CPU_B_MAX_TEMP_OFFSET), 31);
}
static void get_gpu_info()
{
	gpu_info.ttj = sign_extend32(
			readl(thermal_csram_base + GPU_TTJ_OFFSET), 31);
	gpu_info.limit_powerbudget = sign_extend32(
			readl(thermal_csram_base + GPU_POWERBUDGET_OFFSET), 31);
	gpu_info.temp = sign_extend32(
			readl(thermal_csram_base + GPU_TEMP_OFFSET), 31);
	gpu_info.limit_freq = readl(thermal_csram_base + GPU_LIMIT_FREQ_OFFSET);
	gpu_info.cur_freq = readl(thermal_csram_base + GPU_CUR_FREQ_OFFSET);
}
static void get_apu_info()
{
	if (thermal_apu_mbox_base) {
		apu_info.ttj = sign_extend32(
				readl(thermal_apu_mbox_base + APUMBOX_TTJ_OFFSET), 31);
		apu_info.limit_powerbudget = sign_extend32(
				readl(thermal_apu_mbox_base + APUMBOX_POWERBUDGET_OFFSET), 31);
		apu_info.temp = sign_extend32(
				readl(thermal_apu_mbox_base + APUMBOX_TEMP_OFFSET), 31);
		apu_info.limit_opp = sign_extend32(
				readl(thermal_apu_mbox_base + APUMBOX_LIMIT_OPP_OFFSET), 31);
		apu_info.cur_opp = sign_extend32(
				readl(thermal_apu_mbox_base + APUMBOX_CUR_OPP_OFFSET), 31);
	} else {
		apu_info.ttj = sign_extend32(
				readl(thermal_csram_base + APU_TTJ_OFFSET), 31);
		apu_info.limit_powerbudget = sign_extend32(
				readl(thermal_csram_base + APU_POWERBUDGET_OFFSET), 31);
		apu_info.temp = sign_extend32(
				readl(thermal_csram_base + APU_TEMP_OFFSET), 31);
		apu_info.limit_opp = sign_extend32(
				readl(thermal_csram_base + APU_LIMIT_OPP_OFFSET), 31);
		apu_info.cur_opp = sign_extend32(
				readl(thermal_csram_base + APU_CUR_OPP_OFFSET), 31);
	}
}

static enum hrtimer_restart thermal_trace_work(struct hrtimer *timer)
{
	ktime_t ktime;
	int i;
	struct headroom_info hr_info[NR_CPUS];

	if (!thermal_csram_base)
		goto skip;

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

	get_cpu_info();
	get_gpu_info();
	get_apu_info();

	trace_cpu_hr_info_0(&hr_info[0], &hr_info[1], &hr_info[2], &hr_info[3]);
	trace_cpu_hr_info_1(&hr_info[4], &hr_info[5], &hr_info[6], &hr_info[7]);
	trace_thermal_cpu(&cpu_info);
	trace_thermal_gpu(&gpu_info);
	trace_thermal_apu(&apu_info);

skip:
	ktime = ktime_set(0, thermal_trace_data.hr_period);
	hrtimer_forward_now(timer, ktime);

	return HRTIMER_RESTART;
}

static ssize_t enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		thermal_trace_data.enable);

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

	mutex_lock(&thermal_trace_data.lock);
	thermal_trace_data.enable = enable;
	thermal_info_timer_add(thermal_trace_data.enable);
	mutex_unlock(&thermal_trace_data.lock);

	return count;
}

static ssize_t hr_enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		thermal_trace_data.hr_enable);

	return len;
}

static ssize_t hr_enable_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int hr_enable;

	if ((kstrtouint(buf, 10, &hr_enable)))
		return -EINVAL;

	hr_enable = (hr_enable > 0) ? 1 : 0;

	if (hr_enable == thermal_trace_data.hr_enable)
		return count;

	mutex_lock(&thermal_trace_data.lock);
	thermal_trace_data.hr_enable = hr_enable;
	if (thermal_trace_data.hr_enable) {
		thermal_trace_timer_add();
	} else {
		thermal_trace_timer_cancel();
	}
	mutex_unlock(&thermal_trace_data.lock);

	return count;
}

static ssize_t hr_period_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%lu\n",
		thermal_trace_data.hr_period);

	return len;
}

static ssize_t hr_period_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long period;

	if ((kstrtoul(buf, 10, &period)))
		return -EINVAL;

	if (period < 1000000)
		return -EINVAL;

	if (period == thermal_trace_data.hr_period)
		return count;

	mutex_lock(&thermal_trace_data.lock);
	thermal_trace_data.hr_period = period;
	mutex_unlock(&thermal_trace_data.lock);

	return count;
}

static ssize_t mdtput_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%lu\n",
		thermal_info_data.stats[TPUT_MD].cur_tput);

	return len;
}

static ssize_t wifitput_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%lu\n",
		thermal_info_data.stats[TPUT_WIFI].cur_tput);

	return len;
}


static struct kobj_attribute enable_attr = __ATTR_RW(enable);
static struct kobj_attribute hr_enable_attr = __ATTR_RW(hr_enable);
static struct kobj_attribute hr_period_attr = __ATTR_RW(hr_period);
static struct kobj_attribute mdtput_attr = __ATTR_RO(mdtput);
static struct kobj_attribute wifitput_attr = __ATTR_RO(wifitput);
static struct attribute *thermal_trace_attrs[] = {
	&enable_attr.attr,
	&hr_enable_attr.attr,
	&hr_period_attr.attr,
	&mdtput_attr.attr,
	&wifitput_attr.attr,
	NULL
};
static struct attribute_group thermal_trace_attr_group = {
	.name	= "thermal_trace",
	.attrs	= thermal_trace_attrs,
};

static int __init thermal_trace_init(void)
{
	int i, ret;

	thermal_info_data.last_update_time = 0;
	for_each_tput_instance(i) {
		thermal_info_data.stats[i].pre_tx_bytes = 0;
		thermal_info_data.stats[i].cur_tput = 0;
	}
	mutex_init(&thermal_trace_data.lock);
	mutex_init(&thermal_info_data.lock);
	INIT_DELAYED_WORK(&thermal_info_data.poll_queue, thermal_info_work);
	hrtimer_init(&thermal_trace_data.trace_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	thermal_trace_data.trace_timer.function = thermal_trace_work;
	thermal_trace_data.hr_period = 1000000; /* 1ms */

	ret = sysfs_create_group(kernel_kobj, &thermal_trace_attr_group);
	if (ret)
		pr_info("failed to create thermal_trace sysfs, ret=%d!\n", ret);

	return ret;
}
module_init(thermal_trace_init)

static void __exit thermal_trace_exit(void)
{
	mutex_destroy(&thermal_trace_data.lock);
	mutex_destroy(&thermal_info_data.lock);
	sysfs_remove_group(kernel_kobj, &thermal_trace_attr_group);
	cancel_delayed_work_sync(&thermal_info_data.poll_queue);
	hrtimer_cancel(&thermal_trace_data.trace_timer);
}
module_exit(thermal_trace_exit)

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek thermal trace driver");
MODULE_LICENSE("GPL v2");
