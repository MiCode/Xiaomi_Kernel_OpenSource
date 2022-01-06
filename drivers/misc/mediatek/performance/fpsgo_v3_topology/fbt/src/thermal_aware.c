// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/hrtimer.h>
#include <linux/cpumask.h>
#include <linux/workqueue.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include "tscpu_settings.h"
#include "mtk_thermal_monitor.h"
#include "cpu_ctrl.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_common.h"
#include "mtk_thermal.h"

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
	int temp = tscpu_get_curr_max_ts_temp();

	temp /= 1000;

	return temp;
}

static int thrm_get_pcb_temp(void)
{
	int temp = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP);

	temp /= 1000;

	return temp;
}

static void thrm_set_isolation(int input, int cpu)
{
	int nr_cpus = num_possible_cpus();
	int cl;
	int cl_min = -1, cl_max = -1;

	if (cpu >= nr_cpus || cpu < 0)
		return;

	cl = arch_cpu_cluster_id(cpu);
	if (cl >= g_cluster_num || cl < 0)
		return;

	if (input == THRM_AWARE_CORE_ISO)
		cl_max = g_core_num[cl] - 1;
	else if (input == THRM_AWARE_CORE_DEISO)
		cl_min = cl_max = g_core_num[cl];

//#ifdef CONFIG_MTK_CORE_CTL
//	update_cpu_core_limit(CPU_ISO_KIR_FPSGO, cl, cl_min, cl_max);
//#else
//	update_isolation_cpu(CPU_ISO_KIR_FPSGO, input ? 1 : -1, cpu);
//#endif
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
		fpsgo_systrace_c_fbt_gm(-100, 0, cur_state, "thrm_aware_state");
	}
}

static void thrm_check_status(struct work_struct *work)
{
	int temp;
	unsigned long long cur_time;

	mutex_lock(&thrm_aware_lock);

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
	fpsgo_systrace_c_fbt_gm(-100, 0, cur_state, "thrm_aware_state");

EXIT:
	mutex_unlock(&thrm_aware_lock);
}

void thrm_aware_frame_start(int perf_hint, int target_fps)
{
	int temp;
	unsigned long long cur_time;

	mutex_lock(&thrm_aware_lock);

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
			fpsgo_systrace_c_fbt_gm(-100, 0, cur_state, "thrm_aware_state");
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

	if (enable == thrm_aware_enable)
		goto EXIT;

	temp = thrm_get_pcb_temp();

	if (!enable && temp < iso_pcbtemp_th) {
		thrm_aware_enable = 0;

		if (cur_state != THRM_AWARE_STATE_ACTIVE)
			goto EXIT;

		cur_state = THRM_AWARE_STATE_TRANSITION;
		fpsgo_systrace_c_fbt_gm(-100, 0, cur_state, "thrm_aware_state");

		if (limit_cpu != -1)
			thrm_set_isolation(THRM_AWARE_CORE_DEISO, limit_cpu);

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
static void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->package_id == cluster_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

static void update_cpu_info(void)
{
	int i;

	g_cluster_num = arch_nr_clusters();
	g_core_num = kcalloc(g_cluster_num, sizeof(int), GFP_KERNEL);

	for (i = 0; i < g_cluster_num; i++) {
		struct cpumask cluster_cpus;

		arch_get_cluster_cpus(&cluster_cpus, i);
		g_core_num[i] = cpumask_weight(&cluster_cpus);
	}
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

	update_cpu_info();

	thrm_aware_kobj = dir_kobj;

	if (!thrm_aware_kobj)
		return;

	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_temp_th);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_limit_cpu);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_sub_cpu);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_activate_fps);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_iso_pcbtemp_th);
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
}

