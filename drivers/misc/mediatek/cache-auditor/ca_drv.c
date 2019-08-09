/*
 * Copyright (C) 2018 MediaTek Inc.
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

#undef pr_fmt
#define pr_fmt(fmt) "Cache-Auditor: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/perf_event.h>
#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/cgroup-defs.h>
#include <linux/cgroup.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>


#include "internal.h"

#define CREATE_TRACE_POINTS
#include "trace_cache_auditor.h"

enum CA55_PMU_EVENT {
	FRONTEND_STALL,
	BACKEND_STALL,
	CPU_CYCLES,
	LL_CACHE_MISS_RD,
	INST_RETIRED,
	NR_PMU_COUNTERS
};
int ca55_register[] = {
	[FRONTEND_STALL] = 0x23,
	[BACKEND_STALL] = 0x24,
	[CPU_CYCLES] = 0x1d,
	[LL_CACHE_MISS_RD] = 0x37,
	[INST_RETIRED] = 0x08,
};
int ca75_register[] = {
	[FRONTEND_STALL] = 0x23,
	[BACKEND_STALL] = 0x24,
	[CPU_CYCLES] = 0x1d,
	[LL_CACHE_MISS_RD] = 0x37,
	[INST_RETIRED] = 0x08,
};

static DEFINE_PER_CPU(struct ca_pmu_stats, ca_pmu_stats);

/* true, is pmu counter enabled */
static int is_ca_enabled;
static bool force_stop;

/* true, is perf_event alloc done */
static bool is_events_created;

static bool ctl_partition_enable = true;
module_param(ctl_partition_enable, bool, 0600);

static DEFINE_MUTEX(ca_pmu_lock);

__read_mostly unsigned long audit_until;
__read_mostly int in_auditing;

module_param(audit_until, ulong, 0600);

unsigned long ctl_stall_ratio = 300; // 30.0%
module_param(ctl_stall_ratio, ulong, 0600);

unsigned long ctl_background_badness = 10;
module_param(ctl_background_badness, ulong, 0600);

unsigned int ctl_penalty_shift = 14;
module_param(ctl_penalty_shift, uint, 0600);

static inline int get_group_id(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_CGROUP_SCHEDTUNE)
	const int subsys_id = schedtune_cgrp_id;
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(task, subsys_id);
	rcu_read_unlock();
	return grp->id;
#else
	return 0;
#endif
}

static inline bool is_important(struct task_struct *task)
{
	int grp_id = get_group_id(task);

	return (grp_id == GROUP_TA);
}

static inline bool is_background(struct task_struct *task)
{
	return !is_important(task);
}

/*
 * Profiling and audit previous task
 *
 * Step 1. Only consider fair_sched_class
 * Step 2.
 *   For foregound tasks: check stall ratio.
 *   For background tasks: Audit badness (LL-cache dirty behavior)
 *
 */
static inline void aggregate_prev_task(unsigned long now,
		struct task_struct *prev)
{
	int cpu = smp_processor_id();
	struct ca_pmu_stats *cp_stats = &per_cpu(ca_pmu_stats, cpu);
	u64 *cp_counters = cp_stats->prev_counters;
	unsigned long long delta;

	if (cp_stats->in_aging_control) {
		cp_stats->in_aging_control = false;
		release_cache_control(cpu);
		trace_ca_apply_control(cpu, 0);
	} else if (cp_stats->in_partition_control) {
		cp_stats->in_partition_control = false;
		config_partition(0);
		trace_ca_apply_control(cpu, 0);
	}
	if (prev->sched_class == &fair_sched_class
			&& cp_stats->prev_clock_task) {
		u64 stall_ratio, badness;
		int i;
		unsigned long loadwop_avg, nr_stall;

		loadwop_avg = prev->se.avg.loadwop_avg;
		delta = now - cp_stats->prev_clock_task;

		/* dump all pmu counters */
		for (i = 0; i < NR_PMU_COUNTERS; i++) {
			struct perf_event *event = cp_stats->events[i];
			u64 counter;

			if (!event || event->state != PERF_EVENT_STATE_ACTIVE)
				continue;

			counter = perf_event_read_local(event);
			cp_counters[i] = counter - cp_counters[i] + 1;
			trace_ca_aggregate_prev(delta, event, cp_counters[i]);
		}
		trace_ca_exec_summary(delta, cp_counters);
		/*
		 * stall_ratio: stall cycles / total cycles
		 *
		 *               L2 miss count      L3 cache miss count
		 * badness:     ---------------- * ---------------------
		 *               L2 access cnt.         inst. count
		 */
		nr_stall = cp_counters[FRONTEND_STALL];
		nr_stall += cp_counters[BACKEND_STALL];
		stall_ratio = nr_stall * 1024 /	cp_counters[CPU_CYCLES];
		badness = (cp_counters[LL_CACHE_MISS_RD] * loadwop_avg)/
			(cp_counters[INST_RETIRED]);
		prev->stall_ratio = stall_ratio;
		prev->badness = badness;

		if (is_important(prev) &&
			(stall_ratio > ctl_stall_ratio)) {
			unsigned long next_time;
			unsigned long penalty;

			penalty = stall_ratio - ctl_stall_ratio;
			penalty = penalty << ctl_penalty_shift;
			next_time = now + penalty;

			if (time_after((next_time), audit_until)) {
				audit_until = next_time;
				if (!in_auditing) {
					trace_ca_audit_cache(true);
					in_auditing = true;
				}
			}
		}

		trace_ca_print_stall(prev, get_group_id(prev), stall_ratio);
		trace_ca_print_badness(prev, get_group_id(prev), badness);
	} else
		trace_ca_print_badness(prev, get_group_id(prev), 0);
}

inline void audit_next_task(unsigned long now, struct task_struct *next)
{
	int cpu = smp_processor_id();
	struct ca_pmu_stats *cp_stats = &per_cpu(ca_pmu_stats, cpu);
	int i;

	cp_stats->prev_clock_task = now;
	for (i = 0; i < NR_PMU_COUNTERS; i++) {
		struct perf_event *event = cp_stats->events[i];
		u64 *cp_counters = cp_stats->prev_counters;

		if (event && event->state == PERF_EVENT_STATE_ACTIVE)
			cp_counters[i] = perf_event_read_local(event);
	}

	if (!in_auditing)
		return;

	// Auditing expired
	if (time_after(now, audit_until)) {
		in_auditing = false;
		trace_ca_audit_cache(false);
	}

	// not background, no need to control
	if (!is_background(next))
		return;
	// DO NOT audit kernel threads
	if (!next->mm)
		return;

	/*
	 * In this condition, the subset is:
	 *   - In auditiing time zone
	 *   - Is background
	 * Than check where the badness need to be enforced
	 */
	if (ctl_partition_enable &&
			next->badness > ctl_background_badness * 2) {
		cp_stats->in_partition_control = true;
		config_partition(1);
		trace_ca_apply_control(cpu, next->badness);
	} else if (next->badness > ctl_background_badness) {
		cp_stats->in_aging_control = true;
		apply_cache_control(cpu);
		trace_ca_apply_control(cpu, next->badness);
	}
}

void hook_ca_context_switch(struct rq *rq, struct task_struct *prev,
		    struct task_struct *next)
{
	unsigned long now;

	if (is_ca_enabled == 0)
		return;

	now = sched_clock();
	aggregate_prev_task(now, prev);
	audit_next_task(now, next);
}
EXPORT_SYMBOL(hook_ca_context_switch);

static void ca_pmu_event_disable(void)
{
	int cpu, i;
	struct perf_event *event;

	for_each_online_cpu(cpu) {
		if (cpu >= num_possible_cpus())
			break;

		for (i = 0; i < NR_PMU_COUNTERS; i++) {
			event = per_cpu(ca_pmu_stats, cpu).events[i];

			if (!event)
				break;
			perf_event_disable(event);
		}
	}
}

static void ca_pmu_overflow_handler(struct perf_event *event,
			struct perf_sample_data *data, struct pt_regs *regs)
{
	unsigned long long count = local64_read(&event->count);

	pr_info("[Overflow]: ignoring spurious overflow on cpu %u,config=%llu count=%llu\n",
	       event->cpu, event->attr.config, count);
}

static inline void ca_pmu_toggle_cpu_locked(int cpu, int enable)
{
	struct perf_event **events;
	int i;

	events = per_cpu(ca_pmu_stats, cpu).events;

	for (i = 0; i < NR_PMU_COUNTERS; i++) {
		if (!events[i])
			break;

		if (enable)
			perf_event_enable(events[i]);
		else
			perf_event_disable(events[i]);
	}
}

static void ca_pmu_toggle_cpu(int cpu, bool enable)
{
	mutex_lock(&ca_pmu_lock);

	if (is_ca_enabled > 0) {
		mutex_unlock(&ca_pmu_lock);
		return;
	}
	ca_pmu_toggle_cpu_locked(cpu, enable);

	mutex_unlock(&ca_pmu_lock);
}

static int ca_pmu_create_counter(int cpu)
{
	int i;
	int *configs;
	struct perf_event_attr attr = {
		.type           = PERF_TYPE_RAW,
		.size           = sizeof(struct perf_event_attr),
		.pinned         = 1,
		.sample_period  = 0,
	};
	configs = per_cpu(ca_pmu_stats, cpu).config;

	for (i = 0; i < NR_PMU_COUNTERS; i++) {
		struct perf_event *event = per_cpu(ca_pmu_stats, cpu).events[i];

		if (event)
			break;

		attr.config = configs[i];
		event = perf_event_create_kernel_counter(&attr, cpu,
			NULL, ca_pmu_overflow_handler, NULL);

		if (IS_ERR(event)) {
			pr_info("error code: %lu\n", PTR_ERR(event));
			goto error;
		}
		per_cpu(ca_pmu_stats, cpu).events[i] = event;
	}

	return 0;
error:
	pr_info("%s: [ERROR] in CPU%d, i=%d, config=%x\n",
			__func__, cpu, i, configs[i]);
	return -1;
}

static void ca_pmu_event_enable(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		if (cpu >= num_possible_cpus())
			break;

		if (cpu_online(cpu))
			ca_pmu_toggle_cpu_locked(cpu, true);
	}
}

int ca_notifier(struct notifier_block *self,
			    unsigned long action, void *hcpu)
{
	int cpu = (long)hcpu;

	switch (action) {
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		ca_pmu_toggle_cpu(cpu, true);
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		ca_pmu_toggle_cpu(cpu, false);
		break;
	}

	return NOTIFY_OK;
}

struct state_partition_stats {
	int state[CONFIG_NR_CPUS];
};

void dump_partition_config_per_cpu(void *info)
{
	struct state_partition_stats *pstats;
	int cpu = smp_processor_id();

	register int val asm("x3");

	pstats = (struct state_partition_stats *)info;

	if (IS_ERR(pstats) || !pstats) {
		pr_info("%s: [Error] Invalid info address\n", __func__);
		return;
	}
	asm volatile(
			"mrs	%0, s3_0_c15_c4_0\n"
			: "=&r"(val)
	);
	pstats->state[cpu] = val;
}

/* PROCFS */
static int ca_debug_proc_show(struct seq_file *m, void *v)
{
	int i, cpu;
	struct perf_event *event;
	struct state_partition_stats pstats;

	seq_printf(m, "%s: show\n", __func__);
	seq_puts(m, "ca55 configs = [");
	for (i = 0; i < NR_PMU_COUNTERS; i++)
		seq_printf(m, "0x%x, ", ca55_register[i]);
	seq_puts(m, "]\n");
	seq_puts(m, "ca75 configs = [");
	for (i = 0; i < NR_PMU_COUNTERS; i++)
		seq_printf(m, "0x%x, ", ca75_register[i]);
	seq_puts(m, "]\n");

	for_each_online_cpu(cpu) {
		seq_printf(m, "cpu%d {\n", cpu);
		for (i = 0; i < NR_PMU_COUNTERS; i++) {
			event = per_cpu(ca_pmu_stats, cpu).events[i];
			seq_printf(m, "\tconfig[%d]: 0x%llx (event=%p, state=%d)\n",
					i, event->attr.config,
					event, event->state);
		}
		seq_puts(m, "}\n");
	}
	seq_puts(m, "Dump ARM's L3 partition register.\n");
	for (i = 0; i < nr_cpu_ids; i++)
		smp_call_function_single(i,
				dump_partition_config_per_cpu, &pstats, 1);

	for (i = 0; i < nr_cpu_ids; i++) {
		if (pstats.state[i] == 3) {
			seq_printf(m, "ERR! CPU%d get val: %d",
					i, pstats.state[i]);
			continue;
		}
		seq_printf(m, "\tCPU%d: group[%d]\n", i, pstats.state[i]);
	}
	return 0;
}

static int ca_debug_proc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, ca_debug_proc_show, NULL);
}

static const struct file_operations ca_debug_procfs = {
		.owner	 = THIS_MODULE,
		.open	 = ca_debug_proc_open,
		.read	 = seq_read,
		.llseek	 = seq_lseek,
		.release = single_release,
};

static int ca_set_cache_control(const char *buf, const struct kernel_param *kp)
{
	int ret = 0;
	int val, paranoid_backup;
	unsigned long long start, end;

	start = sched_clock();
	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (!is_events_created)
		return -EAGAIN;

	mutex_lock(&ca_pmu_lock);

	if (force_stop) {
		pr_debug("Stopped by force_stop property. (Please config to false to enable)\n");
		mutex_unlock(&ca_pmu_lock);
		return -EAGAIN;
	}

	if (val == is_ca_enabled) {
		pr_debug("No need to change config\n");
		mutex_unlock(&ca_pmu_lock);
		return 0;
	}

	pr_info("Changing state from %d to %d ...\n", is_ca_enabled, val);

	// backup paranoid and set to 0 for pmu counter permission
	paranoid_backup = sysctl_perf_event_paranoid;
	sysctl_perf_event_paranoid = 0;

	if (val == 0) {
		ret = param_set_int(buf, kp);
		ca_pmu_event_disable();

	} else if (val > 0) {
		struct default_setting *setting;

		/*
		 * if state change from disable to enable,
		 * enable PMU events then write to proper setting
		 */
		if (is_ca_enabled == 0)
			ca_pmu_event_enable();

		if (val < NR_TYPES) {
			setting = &set_mode[val];
			ctl_partition_enable   = setting->partition_enable;
			ctl_penalty_shift      = setting->penalty_shift;
			ctl_stall_ratio        = setting->stall_ratio;
			ctl_background_badness = setting->background_badness;
		}
		ret = param_set_int(buf, kp);
	}

	sysctl_perf_event_paranoid = paranoid_backup;

	mutex_unlock(&ca_pmu_lock);
	if (ret < 0)
		return ret;
	end = sched_clock();
	pr_info("[Change complete] %s with %llu ns with mode %d\n",
			is_ca_enabled ? "enable":"disable",
			(end - start), is_ca_enabled);
	return 0;
}

struct kernel_param_ops ca_enable_cb = {
	/* Returns 0, or -errno.  arg is in kp->arg. */
	.set = ca_set_cache_control,
	/* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
	.get = param_get_int,
};
param_check_int(enabled, &is_ca_enabled);
module_param_cb(enable, &ca_enable_cb, &is_ca_enabled, 0664);
__MODULE_PARM_TYPE(enabled, "int");

int ca_force_stop_set_in_kernel(int val)
{
	int paranoid_backup;

	if (!is_events_created)
		return -EAGAIN;

	mutex_lock(&ca_pmu_lock);

	if (val == force_stop) {
		pr_debug("No need to change force_stop config\n");
		mutex_unlock(&ca_pmu_lock);
		return 0;
	}

	pr_info("Change force_stop setting [%d -> %d], is_ca_enabled:%d from:%pS\n",
		force_stop, val, is_ca_enabled, __builtin_return_address(0));

	// backup paranoid and set to 0 for pmu counter permission
	paranoid_backup = sysctl_perf_event_paranoid;
	sysctl_perf_event_paranoid = 0;

	if (val == true && is_ca_enabled > 0) {
		ca_pmu_event_disable();
		is_ca_enabled = 0;
	}
	force_stop = val;

	sysctl_perf_event_paranoid = paranoid_backup;

	mutex_unlock(&ca_pmu_lock);
	pr_info("[Change complete] force_stop:%d\n", force_stop);
	return 0;
}

static int ca_force_stop_set(const char *buf, const struct kernel_param *kp)
{
	int ret = 0;
	bool val;
	int paranoid_backup;

	ret = kstrtobool(buf, &val);
	if (ret < 0)
		return ret;

	if (!is_events_created)
		return -EAGAIN;

	mutex_lock(&ca_pmu_lock);

	if (val == force_stop) {
		pr_debug("No need to change force_stop config\n");
		mutex_unlock(&ca_pmu_lock);
		return 0;
	}

	pr_info("Change force_stop setting [%d -> %d], is_ca_enabled:%d\n",
			force_stop, val, is_ca_enabled);

	// backup paranoid and set to 0 for pmu counter permission
	paranoid_backup = sysctl_perf_event_paranoid;
	sysctl_perf_event_paranoid = 0;

	if (val == true && is_ca_enabled > 0) {
		ca_pmu_event_disable();
		is_ca_enabled = 0;
	}
	ret = param_set_bool(buf, kp);

	sysctl_perf_event_paranoid = paranoid_backup;

	mutex_unlock(&ca_pmu_lock);
	if (ret < 0)
		return ret;
	pr_info("[Change complete] force_stop:%d\n", force_stop);
	return 0;
}

struct kernel_param_ops force_stop_cb = {
	/* Returns 0, or -errno.  arg is in kp->arg. */
	.set = ca_force_stop_set,
	/* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
	.get = param_get_bool,
};
param_check_bool(force_stop, &force_stop);
module_param_cb(force_stop, &force_stop_cb, &force_stop, 0664);
__MODULE_PARM_TYPE(force_stop, "bool");

static const struct cpu_config_node config_table[] = {
	{ "arm,cortex-a75", ca75_register},
	{ "arm,cortex-a55", ca55_register},
	{ NULL, },
};

static void __init init_cpu_config(void)
{
	const struct cpu_config_node *config_index;
	struct device_node *cn = NULL;
	int cpu;

	for_each_possible_cpu(cpu) {
		cn = of_get_cpu_node(cpu, NULL);

		if (!cn) {
			pr_info("Missing device node for CPU %d\n", cpu);
			continue;
		}

		for (config_index = config_table;
				config_index->compatible; config_index++)
			if (of_device_is_compatible(cn,
						config_index->compatible))
				break;

		if (config_index->compatible == NULL) {
			pr_info("No compatible register for cpu%d: %s\n",
					cpu, config_index->compatible);
			continue;
		}
		per_cpu(ca_pmu_stats, cpu).config = config_index->config;
		pr_debug("%s: Init cpu%d with %pf", __func__,
				cpu, config_index->config);
	}
	pr_info("%s: [Done]\n", __func__);
}

static int __init ca_init(void)
{
	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};
	int cpu, ret;

	if (!proc_create("ca_debug", 0444, NULL, &ca_debug_procfs)) {
		pr_info("[Error] create /proc/ca_debug failed\n");
		return -1;
	}

	init_cache_priority();
	init_cpu_config();
	for_each_online_cpu(cpu) {

		if (cpu >= num_possible_cpus())
			break;

		ret = ca_pmu_create_counter(cpu);
		if (ret) {
			pr_info("%s: [ERROR] ca_pmu_create_counter(CPU%d)\n",
					__func__, cpu);
			return ret;
		}
	}
	is_events_created = true;
	if (register_cpu_notifier(&ca_nb))
		pr_info("%s: [Error] fail to register_cpu_notifier.\n",
				__func__);
	else
		pr_info("%s: [Done]\n", __func__);

	return 0;
}

static void __exit ca_exit(void)
{
	int i, cpu;
	struct perf_event *event;

	if (!is_events_created)
		return;

	unregister_cpu_notifier(&ca_nb);

	mutex_lock(&ca_pmu_lock);
	if (is_ca_enabled)
		ca_pmu_event_disable();
	mutex_unlock(&ca_pmu_lock);

	for_each_online_cpu(cpu) {

		if (cpu >= num_possible_cpus())
			break;

		for (i = 0; i < NR_PMU_COUNTERS; i++) {
			event = per_cpu(ca_pmu_stats, cpu).events[i];

			if (!event)
				break;
			perf_event_release_kernel(event);
		}
	}
}

module_init(ca_init);
module_exit(ca_exit);

