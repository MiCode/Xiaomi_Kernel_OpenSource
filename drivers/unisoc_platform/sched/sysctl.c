// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Unisoc (shanghai) Technologies Co., Ltd
 */

#include <linux/sysctl.h>
#include "uni_sched.h"

static int one_thousand = 1000;

#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
/* map util clamp_min to boost */
unsigned int sysctl_sched_uclamp_min_to_boost = 1;
EXPORT_SYMBOL_GPL(sysctl_sched_uclamp_min_to_boost);
unsigned int sysctl_sched_uclamp_threshold = 100;
#else
unsigned int sysctl_sched_uclamp_threshold = 50;
#endif
EXPORT_SYMBOL_GPL(sysctl_sched_uclamp_threshold);

#ifdef CONFIG_UNISOC_GROUP_BOOST
unsigned int sysctl_sched_spc_threshold = 100;
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
static int one_hundred = 100;
static int two_thousand = 2000;
unsigned int sysctl_walt_account_irq_time = 1;
unsigned int sysctl_sched_long_running_rt_task_ms = CONFIG_UNISOC_RT_TIMEOUT_DETECT_MS;
unsigned int sysctl_sched_rt_task_timeout_panic;
unsigned int sysctl_walt_boost_irqload;
#endif
unsigned int sysctl_sched_task_util_prefer_little;
unsigned int sysctl_cpu_multi_thread_opt;
unsigned int sysctl_multi_thread_heavy_load_runtime = 1000;
unsigned int sysctl_force_newidle_balance = 1;
unsigned int sysctl_sched_custom_scene;
#ifdef CONFIG_UNISOC_INPUT_BOOST
static int one_hundred_thousand = 100000;
unsigned int sysctl_input_boost_ms = 200;
unsigned int sysctl_input_boost_freq[8];
unsigned int sysctl_input_boost_enable;

unsigned int sysctl_powerkey_boost_ms = 400;
unsigned int sysctl_powerkey_boost_freq[8];
unsigned int sysctl_powerkey_boost_enable;
#endif

/*up cap margin default value: ~20%*/
static unsigned int sysctl_sched_cap_margin_up_pct[MAX_CLUSTERS] = {
					[0 ... MAX_CLUSTERS-1] = 80};
/*down cap margin default value: ~20%*/
static unsigned int sysctl_sched_cap_margin_dn_pct[MAX_CLUSTERS]  = {
					[0 ... MAX_CLUSTERS-1] = 80};

unsigned int sched_cap_margin_up[UNI_NR_CPUS] = { [0 ... UNI_NR_CPUS-1] = 1280};
unsigned int sched_cap_margin_dn[UNI_NR_CPUS] = { [0 ... UNI_NR_CPUS-1] = 1280};

#ifdef CONFIG_PROC_SYSCTL
static void sched_update_cap_migrate_values(bool up)
{
	int i = 0, cpu;
	struct sched_cluster *cluster;
	int cap_margin_levels = num_sched_clusters ? num_sched_clusters : 1;

	/* per cluster should have a capacity_margin value. */
	for_each_sched_cluster(cluster) {
		for_each_cpu(cpu, &cluster->cpus) {
			if (up)
				sched_cap_margin_up[cpu] =
					SCHED_FIXEDPOINT_SCALE * 100 /
					sysctl_sched_cap_margin_up_pct[i];
			else
				sched_cap_margin_dn[cpu] =
					SCHED_FIXEDPOINT_SCALE * 100 /
					sysctl_sched_cap_margin_dn_pct[i];
		}
		if (++i >= cap_margin_levels)
			break;
	}
}

/*
 * userspace can use write to set new updowm capacity_margin value.
 * for example. echo 80 90 > sysctl_sched_cap_margin_up
 * echo 70 80 > sysctl_sched_cap_margin_dn.
 */
static int sched_updown_migrate_handler(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos)
{
	int ret, i;
	unsigned int *data = (unsigned int *)table->data;
	static DEFINE_MUTEX(mutex);
	unsigned long cap_margin_levels = num_sched_clusters ? num_sched_clusters : 1;
	unsigned int val[MAX_CLUSTERS];
	struct ctl_table tmp = {
		.data   = &val,
		.maxlen = sizeof(int) * cap_margin_levels,
		.mode   = table->mode,
	};

	mutex_lock(&mutex);

	if (!write) {
		for (i = 0; i < cap_margin_levels; i++)
			val[i] = data[i];

		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);

		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* check if pct values are valid */
	for (i = 0; i < cap_margin_levels; i++) {
		if (val[i] <= 0 || val[i] > 100) {
			ret = -EINVAL;
			goto unlock_mutex;
		}
	}

	for (i = 0; i < cap_margin_levels; i++)
		data[i] = val[i];

	sched_update_cap_migrate_values(data == &sysctl_sched_cap_margin_up_pct[0]);

unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}
#endif

static DEFINE_MUTEX(sysctl_pid_mutex);

static int sched_uclamp_fork_reset_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;
	struct task_struct *task;
	struct uni_task_struct *uni_task;
	int pid  = -1;

	struct ctl_table tmp = {
		.data   = &pid,
		.maxlen = sizeof(pid),
		.mode   = table->mode,
	};

	mutex_lock(&sysctl_pid_mutex);

	if (!write) {
		ret = -ENOENT;
		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	if (pid <= 0) {
		ret = -ENOENT;
		goto unlock_mutex;
	}

	/* parsed the values successfully in pid_and_val[] array */
	task = get_pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!task) {
		ret = -ENOENT;
		goto unlock_mutex;
	}

	uni_task = (struct uni_task_struct *) task->android_vendor_data1;

	uni_task->uclamp_fork_reset = !!((unsigned long)table->data);

	put_task_struct(task);
unlock_mutex:
	mutex_unlock(&sysctl_pid_mutex);
	return ret;
}

#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
enum {
	TASK_BEGIN = 0,
	SCHED_UI_THREAD,
	SCHED_ANIMATOR,
	SCHED_LL_CAMERA,
	SCHED_AUDIO_TASK,
};

static int sysctl_task_read_pid = 1;

static int sched_task_read_pid_handler(struct ctl_table *table, int write,
					void __user *buffer, size_t *lenp,
					loff_t *ppos)
{
	int ret;

	mutex_lock(&sysctl_pid_mutex);
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	mutex_unlock(&sysctl_pid_mutex);

	return ret;
}

static int sched_task_setting_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret, param, val;
	struct task_struct *task;
	struct uni_task_struct *uni_task;
	int pid_and_val[2] = {-1, -1};

	struct ctl_table tmp = {
		.data   = &pid_and_val,
		.maxlen = sizeof(pid_and_val),
		.mode   = table->mode,
	};

	mutex_lock(&sysctl_pid_mutex);

	if (!write) {
		task = get_pid_task(find_vpid(sysctl_task_read_pid), PIDTYPE_PID);
		if (!task) {
			ret = -ENOENT;
			goto unlock_mutex;
		}
		uni_task = (struct uni_task_struct *) task->android_vendor_data1;
		pid_and_val[0] = sysctl_task_read_pid;
		param = (unsigned long)table->data;

		switch (param) {
		case SCHED_UI_THREAD:
			pid_and_val[1] = !!(uni_task->vip_params & SCHED_UI_THREAD_TYPE);
			break;
		case SCHED_ANIMATOR:
			pid_and_val[1] = !!(uni_task->vip_params & SCHED_ANIMATOR_TYPE);
			break;
		case SCHED_LL_CAMERA:
			pid_and_val[1] = !!(uni_task->vip_params & SCHED_LL_CAMERA_TYPE);
			break;
		case SCHED_AUDIO_TASK:
			pid_and_val[1] = !!(uni_task->vip_params & SCHED_AUDIO_TYPE);
			break;
		default:
			ret = -EINVAL;
			goto put_task;
		}
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		goto put_task;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	if (pid_and_val[0] <= 0 || pid_and_val[1] < 0) {
		ret = -ENOENT;
		goto unlock_mutex;
	}

	/* parsed the values successfully in pid_and_val[] array */
	task = get_pid_task(find_vpid(pid_and_val[0]), PIDTYPE_PID);
	if (!task) {
		ret = -ENOENT;
		goto unlock_mutex;
	}

	uni_task = (struct uni_task_struct *) task->android_vendor_data1;
	param = (unsigned long)table->data;
	val = pid_and_val[1];

	switch (param) {
	case SCHED_UI_THREAD:
		if (val)
			uni_task->vip_params |= SCHED_UI_THREAD_TYPE;
		else
			uni_task->vip_params &= ~SCHED_UI_THREAD_TYPE;
		break;
	case SCHED_ANIMATOR:
		if (val)
			uni_task->vip_params |= SCHED_ANIMATOR_TYPE;
		else
			uni_task->vip_params &= ~SCHED_ANIMATOR_TYPE;
		break;
	case SCHED_LL_CAMERA:
		if (val)
			uni_task->vip_params |= SCHED_LL_CAMERA_TYPE;
		else
			uni_task->vip_params &= ~SCHED_LL_CAMERA_TYPE;
		break;
	case SCHED_AUDIO_TASK:
		if (val)
			uni_task->vip_params |= SCHED_AUDIO_TYPE;
		else
			uni_task->vip_params &= ~SCHED_AUDIO_TYPE;
		break;
	default:
		ret = -EINVAL;
	}

	trace_sched_task_setting_handler(task, param, val);

put_task:
	put_task_struct(task);
unlock_mutex:
	mutex_unlock(&sysctl_pid_mutex);
	return ret;
}
#endif
#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
unsigned int sysctl_rotation_enable = 1;
/* default threshold value is 40ms */
unsigned int sysctl_rotation_threshold_ms = 40;

struct ctl_table rotation_table[] = {
	{
		.procname       = "rotation_enable",
		.data           = &sysctl_rotation_enable,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "rotation_threshold_ms",
		.data		= &sysctl_rotation_threshold_ms,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ONE,
		.extra2		= &one_thousand,
	},
	{ },
};
#endif
struct ctl_table sched_table[] = {
#if IS_ENABLED(CONFIG_SCHED_WALT)
	{
		.procname	= "sched_walt_init_task_load_pct",
		.data		= &sysctl_sched_walt_init_task_load_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1         = SYSCTL_ZERO,
		.extra2         = &one_hundred,
	},
	{
		.procname	= "sched_walt_cpu_high_irqload",
		.data		= &sysctl_sched_walt_cpu_high_irqload,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname       = "sched_walt_busy_threshold",
		.data           = &sysctl_walt_busy_threshold,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1         = SYSCTL_ZERO,
		.extra2         = &one_hundred,
	},
	{
		.procname       = "sched_walt_cross_window_util",
		.data           = &sysctl_sched_walt_cross_window_util,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname       = "sched_walt_io_is_busy",
		.data           = &sysctl_walt_io_is_busy,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "walt_account_irq_time",
		.data		= &sysctl_walt_account_irq_time,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_long_running_rt_task_ms",
		.data		= &sysctl_sched_long_running_rt_task_ms,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_long_running_rt_task_ms_handler,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &two_thousand,
	},
	{
		.procname	= "sched_rt_task_timeout_panic",
		.data		= &sysctl_sched_rt_task_timeout_panic,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_custom_scene",
		.data		= &sysctl_sched_custom_scene,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname       = "sched_walt_boost_irqload",
		.data           = &sysctl_walt_boost_irqload,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#endif
	{
		.procname	= "sched_uclamp_threshold",
		.data		= &sysctl_sched_uclamp_threshold,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_uclamp_fork_reset_set",
		.data		=  (int *)1,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_uclamp_fork_reset_handler,
	},
	{
		.procname	= "sched_uclamp_fork_reset_clear",
		.data		= (int *)0,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= sched_uclamp_fork_reset_handler,
	},
	{
		.procname	= "sched_task_util_prefer_little",
		.data		= &sysctl_sched_task_util_prefer_little,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_cpu_multi_thread_opt",
		.data		= &sysctl_cpu_multi_thread_opt,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_force_newidle_balance",
		.data		= &sysctl_force_newidle_balance,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_multi_thread_heavy_load_runtime_ms",
		.data		= &sysctl_multi_thread_heavy_load_runtime,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ONE,
		.extra2		= &one_thousand,
	},
#ifdef CONFIG_UNISOC_GROUP_BOOST
	{
		.procname	= "sched_spc_threshold",
		.data		= &sysctl_sched_spc_threshold,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
#endif
#ifdef CONFIG_PROC_SYSCTL
	{
		.procname	= "sched_cap_margin_up",
		.data		= &sysctl_sched_cap_margin_up_pct,
		.maxlen		= sizeof(unsigned int) * MAX_CLUSTERS,
		.mode		= 0644,
		.proc_handler	= sched_updown_migrate_handler,
	},
	{
		.procname	= "sched_cap_margin_down",
		.data		= &sysctl_sched_cap_margin_dn_pct,
		.maxlen		= sizeof(unsigned int) * MAX_CLUSTERS,
		.mode		= 0644,
		.proc_handler	= sched_updown_migrate_handler,
	},
#endif
#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
	{
		.procname	= "sched_uclamp_min2boost",
		.data		= &sysctl_sched_uclamp_min_to_boost,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#endif
#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
	{
		.procname	= "sched_task_ui",
		.data		= (int *) SCHED_UI_THREAD,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_setting_handler,
	},
	{
		.procname	= "sched_task_animator",
		.data		= (int *) SCHED_ANIMATOR,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_setting_handler,
	},
	{
		.procname	= "sched_task_ll_camera",
		.data		= (int *) SCHED_LL_CAMERA,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_setting_handler,
	},
	{
		.procname	= "sched_task_audio",
		.data		= (int *) SCHED_AUDIO_TASK,
		.maxlen		= sizeof(unsigned int) * 2,
		.mode		= 0644,
		.proc_handler	= sched_task_setting_handler,
	},
	{
		.procname       = "sched_task_read_pid",
		.data           = &sysctl_task_read_pid,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = sched_task_read_pid_handler,
		.extra1         = SYSCTL_ONE,
		.extra2         = SYSCTL_INT_MAX,
	},
#endif
#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
	{
		.procname	= "rotation",
		.mode		= 0555,
		.child		= rotation_table,
	},
#endif
#ifdef CONFIG_UNISOC_GROUP_CTL
	{
		.procname	= "group_ctl",
		.mode		= 0555,
		.child		= boost_table,
	},
#endif
#ifdef CONFIG_UNISOC_INPUT_BOOST
	{
		.procname	= "input_boost_ms",
		.data		= &sysctl_input_boost_ms,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_hundred_thousand,
	},
	{
		.procname	= "input_boost_freq",
		.data		= &sysctl_input_boost_freq,
		.maxlen		= sizeof(unsigned int) * 8,
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname       = "input_boost_enable",
		.data           = &sysctl_input_boost_enable,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "powerkey_boost_ms",
		.data		= &sysctl_powerkey_boost_ms,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_hundred_thousand,
	},
	{
		.procname	= "powerkey_boost_freq",
		.data		= &sysctl_powerkey_boost_freq,
		.maxlen		= sizeof(unsigned int) * 8,
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname       = "powerkey_boost_enable",
		.data           = &sysctl_powerkey_boost_enable,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#endif
	{ }
};

struct ctl_table sched_base_table[] = {
	{
		.procname	= "unisched",
		.mode		= 0555,
		.child		= sched_table,
	},
	{ },
};
