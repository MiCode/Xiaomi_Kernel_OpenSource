/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/hrtimer.h>
#include <linux/cpumask.h>
#include <linux/workqueue.h>
#include "tscpu_settings.h"
#include "cpu_ctrl.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"

#define TIME_1S  1000000000
#define TARGET_UNLIMITED_FPS 240

static int temp_th;
static int debnc_time;
static int limit_cpu;
static int sub_cpu;
static int activate_fps;

static struct kobject *thrm_aware_kobj;
static struct hrtimer hrt;
static struct workqueue_struct *wq;

static int thrm_aware_enable;

static int cur_state;
static unsigned long long last_frame_ts;
static unsigned long long last_active_ts;

static DEFINE_MUTEX(thrm_aware_lock);

static void thrm_check_status(struct work_struct *work);
static DECLARE_WORK(thrm_aware_work, (void *) thrm_check_status);


enum THRM_AWARE_STATE {
	THRM_AWARE_STATE_INACTIVE = 0,
	THRM_AWARE_STATE_ACTIVE,
};

static int thrm_get_temp(void)
{
	int temp = tscpu_get_curr_max_ts_temp();

	temp /= 1000;

	return temp;
}

static void thrm_set_isolation(int input, int cpu)
{
	int nr_cpus = num_possible_cpus();

	if (cpu >= nr_cpus || cpu < 0)
		return;

	update_isolation_cpu(CPU_ISO_KIR_FPSGO, input ? 1 : -1, cpu);
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
			thrm_set_isolation(0, limit_cpu);
		if (sub_cpu != -1)
			thrm_set_isolation(0, sub_cpu);
	}
}

static void thrm_check_status(struct work_struct *work)
{
	int temp;
	unsigned long long cur_time;

	mutex_lock(&thrm_aware_lock);

	if (!thrm_aware_enable)
		goto EXIT;

	if (cur_state != THRM_AWARE_STATE_ACTIVE)
		goto EXIT;

	cur_time = fpsgo_get_time();
	if (last_frame_ts < (cur_time - TIME_1S))
		goto DEISO;

	temp = thrm_get_temp();

	if (temp >= temp_th || (last_active_ts >= (cur_time - TIME_1S))) {
		thrm_enable_timer();
		goto EXIT;
	}

DEISO:
	cur_state = THRM_AWARE_STATE_INACTIVE;
	thrm_set_isolation(0, limit_cpu);

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
			thrm_set_isolation(1, limit_cpu);
			thrm_enable_timer();
		}
		last_active_ts = cur_time;
	}

EXIT:
	mutex_unlock(&thrm_aware_lock);
}

void thrm_aware_switch(int enable)
{
	int is_active = 0;

	mutex_lock(&thrm_aware_lock);

	if (enable == thrm_aware_enable)
		goto EXIT;

	if (!enable) {
		if (cur_state == THRM_AWARE_STATE_ACTIVE)
			is_active = 1;

		thrm_reset();
		thrm_aware_enable = 0;

		if ((sub_cpu != -1) && is_active)
			thrm_set_isolation(1, sub_cpu);
	} else if (limit_cpu != -1) {
		thrm_aware_enable = 1;
		if (sub_cpu != -1)
			thrm_set_isolation(0, sub_cpu);
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
		thrm_set_isolation(0, sub_cpu);

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

	thrm_aware_kobj = dir_kobj;

	if (!thrm_aware_kobj)
		return;

	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_temp_th);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_limit_cpu);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_sub_cpu);
	fpsgo_sysfs_create_file(thrm_aware_kobj, &kobj_attr_thrm_activate_fps);
}

void __exit thrm_aware_exit(void)
{
	thrm_reset();

	if (!thrm_aware_kobj)
		return;

	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_temp_th);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_limit_cpu);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_sub_cpu);
	fpsgo_sysfs_remove_file(thrm_aware_kobj, &kobj_attr_thrm_activate_fps);
}

