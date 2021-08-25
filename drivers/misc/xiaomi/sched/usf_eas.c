/*
 * Copyright (C) 2020 XiaoMi Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>	/* needed by all modules */
#include <linux/init.h>		/* needed by module macros */
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/debugfs.h>
#include <linux/fb.h>

#if defined(CONFIG_DRM_MSM)
#include <linux/msm_drm_notify.h>
#else
#include <linux/notifier.h>
#endif

/* It is low enough to cancel out CPU's load. */
#define BOOST_MIN_V -100

/*
 * It is close to the maximum that
 * cpu_util_freq_{walt, pelt} can
 * take without overflow.
 */
#define BOOST_MAX_V 1000

#define USF_TAG	"[usf_eas]"

extern DEFINE_PER_CPU(int, sched_load_usf);
extern DEFINE_PER_CPU(unsigned long[PID_MAX_DEFAULT], task_hist_nivcsw);
extern void (*adjust_task_pred_demand) (int cpuid, struct task_struct *p,
					struct rq *rq,
					int event);

static int __maybe_unused neg_one_hundred = -100;

static int is_set_sched_dbg;
static int usf_is_screen_on;
static int is_usf_eas_enabled;

int sysctl_sched_usf_up_l0;
int sysctl_sched_usf_down;
int sysctl_sched_usf_non_ux;
static struct kobj_attribute sched_usf_up_l0_r_attr;
static struct kobj_attribute sched_usf_down_r_attr;
static struct kobj_attribute sched_usf_non_ux_r_attr;

static void update_usf_margin(int exp_margin, int cpu_id)
{
	int boost_vt = 0;

	boost_vt = exp_margin;

	if (!exp_margin) {
		per_cpu(sched_load_usf, cpu_id) = 0;
		return;
	}

	if (boost_vt < BOOST_MIN_V)
		per_cpu(sched_load_usf, cpu_id) = BOOST_MIN_V;
	else if (boost_vt > BOOST_MAX_V)
		per_cpu(sched_load_usf, cpu_id) = BOOST_MAX_V;
	else
		per_cpu(sched_load_usf, cpu_id) = boost_vt;
	if (is_set_sched_dbg)
		trace_printk("%s: cpu_id=%d exp_margin=%d margin=%d\n",
			     USF_TAG, cpu_id, exp_margin,
			     per_cpu(sched_load_usf, cpu_id));
}

void adjust_task_pred_demand_impl(int cpuid, struct task_struct *p,
				  struct rq *rq, int event)
{
	/* sysctl_sched_latency/sysctl_sched_min_granularity */
	u32 bl_sw_num = 3;

	if (!is_usf_eas_enabled)
		return;

	if ((p == NULL) || (rq == NULL)) {
		return;
	}

	if (is_idle_task(p) || (event != TASK_UPDATE)
	    || (p->pid > (PID_MAX_DEFAULT - 1))
	    || ((event == TASK_UPDATE) && (!p->on_rq)))
		return;

	if (usf_is_screen_on && !per_cpu(task_hist_nivcsw, cpuid)[p->pid]) {

		if (p->nivcsw > (per_cpu(task_hist_nivcsw, cpuid)[p->pid]
				 + bl_sw_num + 1)) {
			update_usf_margin(sysctl_sched_usf_up_l0, cpuid);
		} else if (p->nivcsw <
			   (per_cpu(task_hist_nivcsw, cpuid)[p->pid]
			    + bl_sw_num - 1)
			   && (rq->nr_running < bl_sw_num)) {
			update_usf_margin(sysctl_sched_usf_down, cpuid);
		} else {
			update_usf_margin(0, cpuid);
		}
	} else if (!p->mm) {
		update_usf_margin(sysctl_sched_usf_non_ux, cpuid);
	}
}

#if defined(CONFIG_DRM_MSM)
static int usf_lcd_notifier(struct notifier_block *nb,
			    unsigned long val, void *data)
{
	struct msm_drm_notifier *evdata = data;
	unsigned int blank;

	if (!evdata || (evdata->id != 0))
		return 0;

	if (val != MSM_DRM_EVENT_BLANK)
		return 0;

	if (evdata->data && val == MSM_DRM_EVENT_BLANK) {
		blank = *(int *) (evdata->data);

		switch (blank) {
		case MSM_DRM_BLANK_POWERDOWN:
			usf_is_screen_on = 0;
			break;

		case MSM_DRM_BLANK_UNBLANK:
			usf_is_screen_on = 1;
			break;
		default:
			break;
		}
	}
	return NOTIFY_OK;
}
#else
static int usf_lcd_notifier(struct notifier_block *nb,
			    unsigned long val, void *data)
{

	struct fb_event *evdata = data;
	unsigned int blank;

	if (!evdata)
		return 0;

	if (val != FB_EVENT_BLANK)
		return 0;

	if (evdata->data && val == FB_EVENT_BLANK) {

		blank = *(int *) (evdata->data);

		switch (blank) {
		case FB_BLANK_POWERDOWN:
			usf_is_screen_on = 0;
			break;

		case FB_BLANK_UNBLANK:
			usf_is_screen_on = 1;
			break;
		default:
			break;
		}
	}
	return NOTIFY_OK;
}
#endif

static struct notifier_block usf_lcd_nb = {
	.notifier_call = usf_lcd_notifier,
	.priority = INT_MAX,
};

static ssize_t show_sched_usf_up_l0_r_info(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 8;

	len +=
	    snprintf(buf + len, max_len - len, "%d\n",
		     sysctl_sched_usf_up_l0);

	return len;
}

static ssize_t store_sched_usf_up_l0_r_info(struct kobject *kobj,
					    struct kobj_attribute *attr,
					    const char *buf, size_t count)
{
	int val = 0;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret) {
		pr_err(USF_TAG "set state fail ret=%d\n", ret);
		return ret;
	}

	if (val >= -100 && val <= 100) {
		sysctl_sched_usf_up_l0 = val;
	}
	return count;
}

static ssize_t show_sched_usf_down_r_info(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 8;

	len +=
	    snprintf(buf + len, max_len - len, "%d\n",
		     sysctl_sched_usf_down);

	return len;
}

static ssize_t store_sched_usf_down_r_info(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   const char *buf, size_t count)
{
	int val = 0;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret) {
		pr_err(USF_TAG "set state fail ret=%d\n", ret);
		return ret;
	}

	if (val >= -100 && val <= 100) {
		sysctl_sched_usf_down = val;
	}
	return count;
}

static ssize_t show_sched_usf_non_ux_r_info(struct kobject *kobj,
					    struct kobj_attribute *attr,
					    char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 8;

	len +=
	    snprintf(buf + len, max_len - len, "%d\n",
		     sysctl_sched_usf_non_ux);

	return len;
}

static ssize_t store_sched_usf_non_ux_r_info(struct kobject *kobj,
					     struct kobj_attribute *attr,
					     const char *buf, size_t count)
{
	int val = 0;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret) {
		pr_err(USF_TAG "set state fail ret=%d\n", ret);
		return ret;
	}

	if (val >= -100 && val <= 100) {
		sysctl_sched_usf_non_ux = val;
	}
	return count;
}

static struct kobj_attribute sched_usf_up_l0_r_attr =
__ATTR(sched_usf_up_l0_r, 0664,
       show_sched_usf_up_l0_r_info,
       store_sched_usf_up_l0_r_info);

static struct kobj_attribute sched_usf_down_r_attr =
__ATTR(sched_usf_down_r, 0664,
       show_sched_usf_down_r_info,
       store_sched_usf_down_r_info);

static struct kobj_attribute sched_usf_non_ux_r_attr =
__ATTR(sched_usf_non_ux_r, 0664,
       show_sched_usf_non_ux_r_info,
       store_sched_usf_non_ux_r_info);

static struct attribute *sched_attrs[] = {
	&sched_usf_up_l0_r_attr.attr,
	&sched_usf_down_r_attr.attr,
	&sched_usf_non_ux_r_attr.attr,
	NULL,
};

static struct attribute_group sched_attr_group = {
	.attrs = sched_attrs,
};

static int scheduler_check_get(void *data, u64 *val)
{
	*val = (u64) is_set_sched_dbg;

	return 0;
}

static int scheduler_check_set(void *data, u64 val)
{
	unsigned long flag = (unsigned long) val;

	if (!flag)
		is_set_sched_dbg = 0;
	else
		is_set_sched_dbg = 1;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(scheduler_check_fops, scheduler_check_get,
			scheduler_check_set, "%llu\n");

static int __init intera_monitor_init(void)
{
	int res = 0;
	struct attribute_group *attr_group;
	struct kobject *kobj;

#if defined(CONFIG_DRM_MSM)
	res = msm_drm_register_client(&usf_lcd_nb);
#else
	res = fb_register_client(&usf_lcd_nb);
#endif
	if (res < 0)
		pr_err("Failed to register usf_lcd_nb!\n");

	/*
	 * create a sched in cpu_subsys:
	 * /sys/devices/system/cpu/sched_usf/...
	 */
	attr_group = &sched_attr_group;
	kobj = kobject_create_and_add("sched_usf",
				      &cpu_subsys.dev_root->kobj);

	if (kobj) {
		res = sysfs_create_group(kobj, attr_group);
		if (res)
			kobject_put(kobj);
		else
			kobject_uevent(kobj, KOBJ_ADD);
	}

	is_set_sched_dbg = 0;
	debugfs_create_file("scheduler_check",
			    0660, NULL, NULL, &scheduler_check_fops);
	is_usf_eas_enabled = 1;
	sysctl_sched_usf_up_l0 = 0;
	sysctl_sched_usf_down = 0;
	sysctl_sched_usf_non_ux = 0;
	adjust_task_pred_demand = &adjust_task_pred_demand_impl;

	return res;
}

module_init(intera_monitor_init);

static void __exit intera_monitor_exit(void)
{
#if defined(CONFIG_DRM_MSM)
	msm_drm_unregister_client(&usf_lcd_nb);
#else
	fb_unregister_client(&usf_lcd_nb);
#endif
	is_usf_eas_enabled = 0;
}

module_exit(intera_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("XiaoMi USF EAS");
MODULE_AUTHOR("XiaoMi Inc.");
