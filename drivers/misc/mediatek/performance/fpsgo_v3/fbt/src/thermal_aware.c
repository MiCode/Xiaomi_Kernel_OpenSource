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

#define TIME_1S  1000000000
#define TARGET_UNLIMITED_FPS 240

static int temp_th;
static int debnc_time;
static int limit_cpu;
static int sub_cpu;
static int activate_fps;
static int iso_pcbtemp_th;

static struct kobject *thrm_aware_kobj;
static struct hrtimer hrt;
static struct workqueue_struct *wq;

static int thrm_aware_enable;
static int thrm_aware_priority;

static int cur_state;
static unsigned long long last_frame_ts;
static unsigned long long last_active_ts;

static int g_cluster_num;
static int *g_core_num;

static DEFINE_MUTEX(thrm_aware_lock);

static void thrm_check_status(struct work_struct *work);
static DECLARE_WORK(thrm_aware_work, (void *) thrm_check_status);


enum THRM_AWARE_STATE {
	THRM_AWARE_STATE_INACTIVE = 0,
	THRM_AWARE_STATE_ACTIVE,
	THRM_AWARE_STATE_TRANSITION,
};

enum THRM_AWARE_CORE {
	THRM_AWARE_CORE_RESET,
	THRM_AWARE_CORE_ISO,
	THRM_AWARE_CORE_DEISO,
};

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

static void thrm_set_isolation(int input, int cpu)
{
#if IS_ENABLED(CONFIG_MTK_CORE_CTL) && IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	int nr_cpus = num_possible_cpus();
	int cl = -1, i = 0;
	int cl_min = 0, cl_max;

	if (cpu >= nr_cpus || cpu < 0)
		return;

	if (!g_cluster_num)
		return;

	while (cl < (g_cluster_num - 1)) {
		i += g_core_num[cl + 1];
		cl++;
		if (cpu < i)
			break;
	}

	if (cl >= g_cluster_num || cl < 0)
		return;

	if (!g_core_num[cl])
		return;

	cl_max = g_core_num[cl];

	if (input == THRM_AWARE_CORE_ISO)
		cl_max = g_core_num[cl] - 1;
	else if (input == THRM_AWARE_CORE_DEISO)
		cl_min = cl_max = g_core_num[cl];

	fpsgo_systrace_c_fbt_gm(-100, 0, cl_max, "thrm_aware_%d_max", cl);
	fpsgo_systrace_c_fbt_gm(-100, 0, cl_min, "thrm_aware_%d_min", cl);
	fpsgo_main_trace("thrm_aware set cl=%d, max=%d, min=%d", cl, cl_max, cl_min);

	core_ctl_set_limit_cpus(cl, cl_min, cl_max);

#elif IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
	fpsgo_systrace_c_fbt_gm(-100, 0, (input == THRM_AWARE_CORE_ISO)? 1 : 0,
			"thrm_aware_%d", cpu);
	fpsgo_main_trace("thrm_aware set cpu=%d: %d", cpu, input);


	if (input == THRM_AWARE_CORE_ISO)
		sched_pause_cpu(cpu);
	else
		sched_resume_cpu(cpu);
#else
	return;
#endif
}

static void thrm_enable_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, debnc_time);
	hrtimer_start(&hrt, ktime, HRTIMER_MODE_REL);
}

static void thrm_disable_timer(void)
{
	hrtimer_cancel(&hrt);
}

static enum hrtimer_restart thrm_timer_func(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &thrm_aware_work);

	return HRTIMER_NORESTART;
}

static void thrm_reset(void)
{
	last_frame_ts = 0;

	if (cur_state != THRM_AWARE_STATE_INACTIVE) {
		thrm_disable_timer();
		cur_state = THRM_AWARE_STATE_INACTIVE;
		last_active_ts = 0;
		if (limit_cpu != -1)
			thrm_set_isolation(THRM_AWARE_CORE_RESET, limit_cpu);
		if (sub_cpu != -1)
			thrm_set_isolation(THRM_AWARE_CORE_RESET, sub_cpu);
	}
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

	temp = thrm_get_temp();

	if (temp >= temp_th || (last_active_ts >= (cur_time - debnc_time))) {
		thrm_enable_timer();
		goto EXIT;
	}

DEISO:
	cur_state = THRM_AWARE_STATE_INACTIVE;
	last_active_ts = 0;
	thrm_set_isolation(THRM_AWARE_CORE_RESET, limit_cpu);
	thrm_set_isolation(THRM_AWARE_CORE_RESET, sub_cpu);

EXIT:
	mutex_unlock(&thrm_aware_lock);
}

void thrm_aware_frame_start(int perf_hint, int target_fps)
{
	int temp;
	unsigned long long cur_time;

	mutex_lock(&thrm_aware_lock);

	if (!thrm_aware_priority)
		goto EXIT;

	if (!thrm_aware_enable)
		goto EXIT;

	if (perf_hint || (target_fps < activate_fps)) {
		thrm_reset();
		goto EXIT;
	}

	cur_time = fpsgo_get_time();

	temp = thrm_get_temp();

	last_frame_ts = cur_time;

	if (temp >= temp_th) {
		if (cur_state != THRM_AWARE_STATE_ACTIVE) {
			cur_state = THRM_AWARE_STATE_ACTIVE;
			thrm_set_isolation(THRM_AWARE_CORE_ISO, limit_cpu);
			if (sub_cpu != -1)
				thrm_set_isolation(THRM_AWARE_CORE_DEISO, sub_cpu);
			thrm_enable_timer();
		}
		last_active_ts = cur_time;
	}

EXIT:
	mutex_unlock(&thrm_aware_lock);
}

void thrm_aware_switch(int enable)
{
	int temp;

	mutex_lock(&thrm_aware_lock);

	if (!thrm_aware_priority)
		goto EXIT;

	if (enable == thrm_aware_enable)
		goto EXIT;

	temp = thrm_get_pcb_temp();

	if (!enable && temp < iso_pcbtemp_th) {
		thrm_aware_enable = 0;

		if (cur_state != THRM_AWARE_STATE_ACTIVE)
			goto EXIT;

		cur_state = THRM_AWARE_STATE_TRANSITION;

		if (limit_cpu != -1)
			thrm_set_isolation(THRM_AWARE_CORE_RESET, limit_cpu);

		if (sub_cpu != -1)
			thrm_set_isolation(THRM_AWARE_CORE_ISO, sub_cpu);
	} else if (enable && (limit_cpu != -1)) {
		thrm_aware_enable = 1;

		if (sub_cpu != -1)
			thrm_set_isolation(THRM_AWARE_CORE_RESET, sub_cpu);
	}

EXIT:
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

	if (limit_cpu == val)
		goto EXIT;

	thrm_reset();

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

	if (sub_cpu == val)
		goto EXIT;

	if (sub_cpu != -1)
		thrm_set_isolation(THRM_AWARE_CORE_RESET, sub_cpu);

	sub_cpu = val;

EXIT:
	mutex_unlock(&thrm_aware_lock);

	return count;
}

static KOBJ_ATTR_RW(thrm_sub_cpu);

static ssize_t thrm_activate_fps_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int val = -1;

	mutex_lock(&thrm_aware_lock);
	val = activate_fps;
	mutex_unlock(&thrm_aware_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t thrm_activate_fps_store(struct kobject *kobj,
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


	if (val > TARGET_UNLIMITED_FPS || val < 0)
		return count;

	mutex_lock(&thrm_aware_lock);

	if (activate_fps == val)
		goto EXIT;

	if (cur_state != THRM_AWARE_STATE_INACTIVE)
		thrm_reset();

	activate_fps = val;

EXIT:
	mutex_unlock(&thrm_aware_lock);

	return count;
}

static KOBJ_ATTR_RW(thrm_activate_fps);

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
		thrm_reset();

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
		"clus\tnum_cpu\n");
	posi += length;

	for (i = 0; i < g_cluster_num; i++) {
		length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"%d\t%d\n",
			i, g_core_num[i]);
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

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	for_each_possible_cpu(cpu) {
		if (g_cluster_num <= 0 || cluster >= g_cluster_num)
			break;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;

		g_core_num[cluster] = cpumask_weight(policy->cpus);
		cluster++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}
#endif
}

void __init thrm_aware_init(struct kobject *dir_kobj)
{
	hrtimer_init(&hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt.function = &thrm_timer_func;
	wq = create_singlethread_workqueue("thrm_aware");

	temp_th = 65;
	limit_cpu = -1;
	sub_cpu = -1;
	cur_state = THRM_AWARE_STATE_INACTIVE;
	debnc_time = TIME_1S;
	activate_fps = 1;
	iso_pcbtemp_th = 200;
	thrm_aware_priority = 1;

	update_cpu_info();

	thrm_aware_kobj = dir_kobj;

	if (!thrm_aware_kobj)
		return;

	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_temp_th);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_limit_cpu);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_sub_cpu);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_activate_fps);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_iso_pcbtemp_th);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_enable);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_info);
}

void __exit thrm_aware_exit(void)
{
	thrm_reset();

	kfree(g_core_num);

	if (!thrm_aware_kobj)
		return;

	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_temp_th);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_limit_cpu);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_sub_cpu);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_activate_fps);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_iso_pcbtemp_th);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_enable);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_info);
}

