// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/hrtimer.h>
#include <linux/cpumask.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/thermal.h>
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "thermal_aware.h"
#include <mt-plat/fpsgo_common.h>
#include "fbt_cpu_ctrl.h"

#define TIME_1S  1000000000

static int temp_th;
static int debnc_time;
static int limit_cpu;
static int sub_cpu;
static int iso_pcbtemp_th;

static struct kobject *thrm_aware_kobj;
static struct hrtimer hrt;
static struct workqueue_struct *wq;

static int thrm_aware_enable;
static int thrm_aware_priority;

static int cur_state;
static unsigned long long last_frame_ts;
static unsigned long long last_active_ts;
static int cur_pid;
static int cfp_enabled;
static int cur_m_core;

static int g_cluster_num;
static int *g_core_num;
static int *g_first_cpu;

static DEFINE_MUTEX(thrm_aware_lock);

static void thrm_check_status(struct work_struct *work);
static DECLARE_WORK(thrm_aware_work, (void *) thrm_check_status);

#define fpsgo_systrace_c_thrm(val, fmt...) \
	do { \
		if (cur_pid > 0) \
			fpsgo_systrace_c_fbt( \
					cur_pid, 0, val, fmt); \
	} while (0)

enum THRM_AWARE_STATE {
	THRM_AWARE_STATE_INACTIVE = 0,
	THRM_AWARE_STATE_OBSERVE,
	THRM_AWARE_STATE_ACTIVE,
	THRM_AWARE_STATE_TRANSITION,
};

enum THRM_AWARE_CORE {
	THRM_AWARE_CORE_RESET,
	THRM_AWARE_CORE_ISO,
	THRM_AWARE_CORE_DEISO,
};

static int thrm_get_cfp_ceil(void)
{
	if (!cfp_enabled)
		return -1;

	return fbt_cpu_ctrl_get_ceil();
}

static void thrm_cfp_cb(int release)
{
	fpsgo_systrace_c_thrm(release, "thrm_aware_cfp_cb");

	if (wq)
		queue_work(wq, &thrm_aware_work);
}

static void thrm_enable_cfp(int enable)
{
	if (enable == cfp_enabled)
		return;

	if (!enable)
		cfp_mon_disable(CFP_KIR_THER_AWARE);
	else
		cfp_mon_enable(CFP_KIR_THER_AWARE, thrm_cfp_cb);

	cfp_enabled = enable;
}

static int thrm_get_temp(void)
{
#if IS_ENABLED(CONFIG_THERMAL)
	struct thermal_zone_device *zone;
	int ret, temp = 0;

	zone = thermal_zone_get_zone_by_name("soc_max");
	if (IS_ERR(zone))
		return 0;

	ret = thermal_zone_get_temp(zone, &temp);
	if (ret != 0)
		return 0;

	temp /= 1000;

	fpsgo_systrace_c_thrm(temp, "thrm_aware_temp");

	return temp;
#else
	return 0;
#endif
}

static int thrm_get_pcb_temp(void)
{
#if IS_ENABLED(CONFIG_THERMAL)
	struct thermal_zone_device *zone;
	int ret, temp = 0;

	zone = thermal_zone_get_zone_by_name("ap_ntc");
	if (IS_ERR(zone))
		return 0;

	ret = thermal_zone_get_temp(zone, &temp);
	if (ret != 0)
		return 0;

	temp /= 1000;

	return temp;
#else
	return 0;
#endif
}

static void thrm_do_isolation(int cl, int cl_min, int cl_max)
{
	if (cl_max == -1)
		fpsgo_sentcmd(FPSGO_SET_ISOLATION, cl, -1);
	else
		fpsgo_sentcmd(FPSGO_SET_ISOLATION, cl, (cl_max << 4) + cl_min);
}

static int thrm_get_cluster_by_cpu(int cpu)
{
	int cl = -1, i = 0;
	int nr_cpus = num_possible_cpus();

	if (cpu >= nr_cpus || cpu < 0)
		return -1;

	if (!g_cluster_num)
		return -1;

	while (cl < (g_cluster_num - 1)) {
		i += g_core_num[cl + 1];
		cl++;
		if (cpu < i)
			break;
	}

	if (cl >= g_cluster_num || cl < 0)
		return -1;

	return cl;
}

static void thrm_set_isolation_cluster(int input, int cl)
{
	int cl_min = 0, cl_max;

	if (!g_cluster_num)
		return;

	if (cl >= g_cluster_num || cl < 0)
		return;

	if (!g_core_num[cl])
		return;

	if (input == THRM_AWARE_CORE_ISO)
		cl_max = g_core_num[cl] - 1;
	else if (input == THRM_AWARE_CORE_DEISO)
		cl_min = cl_max = g_core_num[cl];
	else
		cl_min = cl_max = -1;

	fpsgo_systrace_c_thrm(cl_max, "thrm_aware_%d_max", cl);
	fpsgo_systrace_c_thrm(cl_min, "thrm_aware_%d_min", cl);
	fpsgo_main_trace("thrm_aware pid=%d, set cl=%d, max=%d, min=%d",
		cur_pid, cl, cl_max, cl_min);

	thrm_do_isolation(cl, cl_min, cl_max);
}

static void thrm_set_isolation(int input, int cpu)
{
	int cl;

	cl = thrm_get_cluster_by_cpu(cpu);
	if (cl == -1)
		return;

	thrm_set_isolation_cluster(input, cl);
}

static void thrm_set_mcpu_isolation(int input)
{
	int cluster;

	if (input == cur_m_core)
		return;

	cluster = g_cluster_num - 2;

	if (cluster <= 0)
		return;

	if (sub_cpu != -1) {
		fpsgo_main_trace("thrm_aware sub_cpu is set %d", sub_cpu);
		return;
	}

	if (limit_cpu >= g_first_cpu[cluster]
		&& limit_cpu < (g_first_cpu[cluster] + g_core_num[cluster])) {
		fpsgo_main_trace("thrm_aware conflict limit_cpu %d", limit_cpu);
		return;
	}

	thrm_set_isolation_cluster(input, cluster);
	cur_m_core = input;
}

static void thrm_enable_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, debnc_time);
	hrtimer_start(&hrt, ktime, HRTIMER_MODE_REL);

	thrm_enable_cfp(1);
}

static void thrm_disable_timer(void)
{
	hrtimer_cancel(&hrt);
	thrm_enable_cfp(0);
}

static enum hrtimer_restart thrm_timer_func(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &thrm_aware_work);

	return HRTIMER_NORESTART;
}

static void thrm_reset(void)
{
	if (cur_state == THRM_AWARE_STATE_INACTIVE
		|| cur_state == THRM_AWARE_STATE_OBSERVE)
		return;

	last_active_ts = 0;
	cur_state = THRM_AWARE_STATE_OBSERVE;
	fpsgo_systrace_c_thrm(cur_state, "thrm_aware_state");

	thrm_set_isolation(THRM_AWARE_CORE_RESET, limit_cpu);
	thrm_set_isolation(THRM_AWARE_CORE_RESET, sub_cpu);
	thrm_set_mcpu_isolation(THRM_AWARE_CORE_RESET);
}

static void thrm_stop(void)
{
	if (cur_state == THRM_AWARE_STATE_INACTIVE)
		return;

	last_frame_ts = 0;
	thrm_reset();
	thrm_disable_timer();
	cur_state = THRM_AWARE_STATE_INACTIVE;
	fpsgo_systrace_c_thrm(cur_state, "thrm_aware_state");
}

static void thrm_check_status(struct work_struct *work)
{
	int temp;
	unsigned long long cur_time;

	mutex_lock(&thrm_aware_lock);

	if (!thrm_aware_priority)
		goto EXIT;

	if (!thrm_aware_enable)
		goto EXIT;

	if (cur_state == THRM_AWARE_STATE_INACTIVE)
		goto EXIT;

	cur_time = fpsgo_get_time();
	if (last_frame_ts < (cur_time - TIME_1S))
		goto DEISO;

	if (cur_state == THRM_AWARE_STATE_OBSERVE)
		goto NEXT;

	temp = thrm_get_temp();
	if ((temp < temp_th && (last_active_ts < (cur_time - debnc_time)))
		|| !thrm_get_cfp_ceil())
		thrm_reset();

NEXT:
	thrm_enable_timer();
	goto EXIT;

DEISO:
	last_active_ts = 0;

	if (cur_state != THRM_AWARE_STATE_OBSERVE) {
		thrm_set_isolation(THRM_AWARE_CORE_RESET, limit_cpu);
		thrm_set_isolation(THRM_AWARE_CORE_RESET, sub_cpu);
		thrm_set_mcpu_isolation(THRM_AWARE_CORE_RESET);
	}

	thrm_enable_cfp(0);
	cur_state = THRM_AWARE_STATE_INACTIVE;
	fpsgo_systrace_c_thrm(cur_state, "thrm_aware_state");

EXIT:
	mutex_unlock(&thrm_aware_lock);
}

static void thrm_set_active(void)
{
	cur_state = THRM_AWARE_STATE_ACTIVE;
	fpsgo_systrace_c_thrm(cur_state, "thrm_aware_state");

	thrm_set_isolation(THRM_AWARE_CORE_ISO, limit_cpu);
	thrm_set_isolation(THRM_AWARE_CORE_DEISO, sub_cpu);
	thrm_set_mcpu_isolation(THRM_AWARE_CORE_DEISO);
}

void thrm_aware_frame_start(int pid, int perf_hint)
{
	int temp;
	unsigned long long cur_time;
	int over_heat = 0;

	mutex_lock(&thrm_aware_lock);

	if (!thrm_aware_priority)
		goto EXIT;

	if (!thrm_aware_enable)
		goto EXIT;

	cur_pid = pid;
	cur_time = fpsgo_get_time();
	last_frame_ts = cur_time;

	if (perf_hint || !thrm_get_cfp_ceil()) {
		thrm_reset();
		goto EXIT;
	}

	temp = thrm_get_temp();
	if (temp >= temp_th) {
		last_active_ts = cur_time;
		over_heat = 1;
	}

	switch (cur_state) {
	case THRM_AWARE_STATE_INACTIVE:
		thrm_enable_timer();
		cur_state = THRM_AWARE_STATE_OBSERVE;
		fpsgo_systrace_c_thrm(cur_state, "thrm_aware_state");
		break;

	case THRM_AWARE_STATE_OBSERVE:
		if (over_heat)
			thrm_set_active();
		break;

	case THRM_AWARE_STATE_TRANSITION:
		thrm_set_active();
		break;

	case THRM_AWARE_STATE_ACTIVE:
	default:
		break;
	}

EXIT:
	mutex_unlock(&thrm_aware_lock);
}

void thrm_aware_switch(void)
{
	int temp;

	mutex_lock(&thrm_aware_lock);

	if (!thrm_aware_priority)
		goto EXIT;

	if (!thrm_aware_enable)
		goto EXIT;

	if (cur_state != THRM_AWARE_STATE_ACTIVE)
		goto EXIT;

	temp = thrm_get_pcb_temp();

	if (temp < iso_pcbtemp_th) {
		cur_state = THRM_AWARE_STATE_TRANSITION;
		fpsgo_systrace_c_thrm(cur_state, "thrm_aware_state");

		thrm_set_isolation(THRM_AWARE_CORE_DEISO, limit_cpu);
		thrm_set_isolation(THRM_AWARE_CORE_ISO, sub_cpu);
	}

EXIT:
	mutex_unlock(&thrm_aware_lock);
}

void thrm_aware_stop(void)
{
	mutex_lock(&thrm_aware_lock);
	thrm_stop();
	cur_pid = 0;
	mutex_unlock(&thrm_aware_lock);
}

static ssize_t thrm_temp_th_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&thrm_aware_lock);
	val = temp_th;
	mutex_unlock(&thrm_aware_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t thrm_temp_th_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	mutex_lock(&thrm_aware_lock);
	temp_th = val;
	mutex_unlock(&thrm_aware_lock);

	return count;
}

static KOBJ_ATTR_RW(thrm_temp_th);

static ssize_t thrm_limit_cpu_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&thrm_aware_lock);
	val = limit_cpu;
	mutex_unlock(&thrm_aware_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t thrm_limit_cpu_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;
	int nr_cpus = num_possible_cpus();

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}


	if (val >= nr_cpus || val < -1)
		return count;

	mutex_lock(&thrm_aware_lock);

	if (g_cluster_num <= 2)
		goto EXIT;

	if (limit_cpu == val)
		goto EXIT;

	thrm_stop();

	limit_cpu = val;

	if (limit_cpu != -1)
		thrm_aware_enable = 1;
	else
		thrm_aware_enable = 0;

EXIT:
	mutex_unlock(&thrm_aware_lock);

	return count;
}

static KOBJ_ATTR_RW(thrm_limit_cpu);

static ssize_t thrm_sub_cpu_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&thrm_aware_lock);
	val = sub_cpu;
	mutex_unlock(&thrm_aware_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t thrm_sub_cpu_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;
	int nr_cpus = num_possible_cpus();

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}


	if (val >= nr_cpus || val < -1)
		return count;

	mutex_lock(&thrm_aware_lock);

	if (g_cluster_num <= 2)
		goto EXIT;

	if (sub_cpu == val)
		goto EXIT;

	if (sub_cpu != -1)
		thrm_set_isolation(THRM_AWARE_CORE_RESET, sub_cpu);
	else {
		thrm_set_mcpu_isolation(THRM_AWARE_CORE_RESET);
		if (cur_state == THRM_AWARE_STATE_ACTIVE)
			thrm_set_isolation(THRM_AWARE_CORE_DEISO, sub_cpu);
	}

	sub_cpu = val;

EXIT:
	mutex_unlock(&thrm_aware_lock);

	return count;
}

static KOBJ_ATTR_RW(thrm_sub_cpu);

static ssize_t thrm_iso_pcbtemp_th_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&thrm_aware_lock);
	val = iso_pcbtemp_th;
	mutex_unlock(&thrm_aware_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t thrm_iso_pcbtemp_th_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	mutex_lock(&thrm_aware_lock);
	iso_pcbtemp_th = val;
	mutex_unlock(&thrm_aware_lock);

	return count;
}

static KOBJ_ATTR_RW(thrm_iso_pcbtemp_th);

static ssize_t thrm_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&thrm_aware_lock);
	val = thrm_aware_priority;
	mutex_unlock(&thrm_aware_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t thrm_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	if (val > 1 || val < 0)
		return count;

	mutex_lock(&thrm_aware_lock);

	if (thrm_aware_priority == val)
		goto EXIT;

	if (!val)
		thrm_stop();

	thrm_aware_priority = val;

EXIT:
	mutex_unlock(&thrm_aware_lock);

	return count;
}

static KOBJ_ATTR_RW(thrm_enable);

static ssize_t thrm_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int posi = 0;
	int length;
	int i;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"enable\tpriority\tstate\ttemp\tpcb_temp\n");
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"%d\t%d\t\t%d\t%d\t%d\n\n",
		thrm_aware_enable, thrm_aware_priority, cur_state,
		thrm_get_temp(), thrm_get_pcb_temp());
	posi += length;

	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"clus\tnum_cpu\tfirst_cpu\n");
	posi += length;

	for (i = 0; i < g_cluster_num; i++) {
		length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"%d\t%d\t%d\n",
			i, g_core_num[i], g_first_cpu[i]);
		posi += length;
	}

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}
static KOBJ_ATTR_RO(thrm_info);

static void update_cpu_info(void)
{
#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	int cluster = 0, cpu;
	struct cpufreq_policy *policy;
#endif

	g_cluster_num = fpsgo_arch_nr_clusters();
	g_core_num = kcalloc(g_cluster_num, sizeof(int), GFP_KERNEL);
	g_first_cpu = kcalloc(g_cluster_num, sizeof(int), GFP_KERNEL);

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	for_each_possible_cpu(cpu) {
		if (g_cluster_num <= 0 || cluster >= g_cluster_num)
			break;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;

		g_core_num[cluster] = cpumask_weight(policy->cpus);
		g_first_cpu[cluster] = cpumask_first(policy->cpus);
		cluster++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}
#endif
}

void __init thrm_aware_init(struct kobject *dir_kobj, int cpu)
{
	hrtimer_init(&hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt.function = &thrm_timer_func;
	wq = create_singlethread_workqueue("thrm_aware");

	update_cpu_info();

	sub_cpu = -1;
	cur_state = THRM_AWARE_STATE_INACTIVE;
	cur_m_core = THRM_AWARE_CORE_RESET;
	debnc_time = TIME_1S;
	iso_pcbtemp_th = 200;
	thrm_aware_priority = 1;

	if (g_cluster_num > 2 && cpu >= 0 && cpu < num_possible_cpus()) {
		temp_th = 0;
		limit_cpu = cpu;
		thrm_aware_enable = 1;
	} else {
		temp_th = 65;
		limit_cpu = -1;
		thrm_aware_enable = 0;
	}

	thrm_aware_kobj = dir_kobj;

	if (!thrm_aware_kobj)
		return;

	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_temp_th);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_limit_cpu);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_sub_cpu);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_iso_pcbtemp_th);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_enable);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_info);
}

void __exit thrm_aware_exit(void)
{
	thrm_stop();

	kfree(g_core_num);
	kfree(g_first_cpu);

	if (!thrm_aware_kobj)
		return;

	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_temp_th);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_limit_cpu);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_sub_cpu);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_iso_pcbtemp_th);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_enable);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_info);
}

