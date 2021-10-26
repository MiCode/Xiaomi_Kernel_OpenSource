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
#include <linux/notifier.h>


#include "internal.h"

#define CREATE_TRACE_POINTS
#include "trace_cache_auditor.h"

int ca55_register[] = {
	[L2_PF_ST] = 0x12c,
	[L2_PF_UNUSED] = 0x150,
	// [L2_PF] = 0x148,
	// [L2_PF_LD] = 0x12b,
	// [L2_PF_REFILL] = 0x10a,
};
int ca75_register[] = {
	[L2_PF_ST] = 0x12c,
	[L2_PF_UNUSED] = 0x150,
	// [L2_PF] = 0x148,
	// [L2_PF_LD] = 0x12b,
	// [L2_PF_REFILL] = 0x10a,
};
int ca76_register[] = {
	[L2_PF_ST] = 0x12c,
	[L2_PF_UNUSED] = 0x150,
};

DEFINE_PER_CPU(struct ca_pmu_stats, ca_pmu_stats);

static bool force_stop;
static int ca_hp_online = -1;

/* true, if perf_event alloc done */
static bool is_events_created;

static DEFINE_MUTEX(ca_pmu_lock);

void hook_ca_scheduler_tick(int cpu)
{
	if (!is_events_created || !pftch_env.is_enabled)
		return;

	pftch_qos_tick(cpu);
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
		.disabled	= 1,
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

static void ca_pmu_event_disable(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		if (cpu >= num_possible_cpus())
			break;
		if (cpu_online(cpu))
			ca_pmu_toggle_cpu_locked(cpu, false);
	}
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

int ca_cpu_online(unsigned int cpu)
{
	ca_pmu_toggle_cpu(cpu, true);
	return 0;
}

int ca_cpu_offline(unsigned int cpu)
{
	ca_pmu_toggle_cpu(cpu, false);
	return 0;
}

struct state_partition_stats {
	int state[CONFIG_NR_CPUS];
};

void dump_preftech_config_per_cpu(void *info)
{
	struct state_partition_stats *pstats;
	int cpu = smp_processor_id();

	unsigned long val = 5566;

	pstats = (struct state_partition_stats *)info;

	if (IS_ERR(pstats) || !pstats) {
		pr_info("%s: [Error] Invalid info address\n", __func__);
		return;
	}
	asm volatile(
			"mrs	%0, s3_0_c15_c1_4\n"
			: "=r"(val)
	);
	if (IS_BIG_CORE(cpu))
		pstats->state[cpu] = (val & 0x80a0);
	else
		pstats->state[cpu] = (val >> 10) & 0x7;
}

/* PROCFS */
static int ca_debug_proc_show(struct seq_file *m, void *v)
{
	int i, cpu;
	struct perf_event *event;
	struct state_partition_stats pstats;

	unsigned long val = 5566;

	asm volatile(
			"mrs    %0, s3_0_c15_c1_4\n"
			: "=r"(val)
	);
	seq_printf(m, "%s: show [%lu]\n", __func__, val);


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

	seq_puts(m, "Dump ARM's preftech register.\n");
	for (i = 0; i < nr_cpu_ids; i++) {
		smp_call_function_single(i,
				dump_preftech_config_per_cpu, &pstats, 1);
	}
	for (i = 0; i < nr_cpu_ids; i++) {
		int nr_caches = -1;

		if (IS_BIG_CORE(i)) {
			seq_printf(m, "\tCPU%d: PF_DIS[15]: %s, PF_STS_DIS[7]:%s",
					i, pstats.state[i] & 0x4 ? "off" : "on",
					pstats.state[i] & 0x2 ? "off" : "on");
			seq_printf(m, ", RPF_DIS[5]: %s\n",
					pstats.state[i] & 0x1 ? "off" : "on");
		} else {
			if (pstats.state[i] == 4)
				nr_caches = 0;
			else if ((pstats.state[i] + 4) % 8 < 6)
				nr_caches = 1 << ((pstats.state[i] + 4) % 8);

			seq_printf(m, "\tCPU%d: 0x%x (%d cache lines)\n", i,
					pstats.state[i], nr_caches);
		}
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


int set_pftch_qos_control(const char *buf, const struct kernel_param *kp)
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
	if (val == pftch_env.is_enabled) {
		pr_debug("No need to change config\n");
		mutex_unlock(&ca_pmu_lock);
		return 0;
	}
	pr_info("Changing state from %d to %d ...\n",
			pftch_env.is_enabled, val);
	// backup paranoid and set to 0 for pmu counter permission
	paranoid_backup = sysctl_perf_event_paranoid;
	sysctl_perf_event_paranoid = 0;
	if (val == 0) {
		ret = param_set_int(buf, kp);
		ca_pmu_event_disable();
	} else if (val > 0) {
		if (pftch_env.is_enabled == 0)
			ca_pmu_event_enable();
		ret = param_set_int(buf, kp);
	}
	sysctl_perf_event_paranoid = paranoid_backup;
	mutex_unlock(&ca_pmu_lock);
	if (ret < 0)
		return ret;
	end = sched_clock();
	pr_info("[Change complete] %s with %llu ns with mode %d\n",
			pftch_env.is_enabled ? "enable":"disable",
			(end - start), pftch_env.is_enabled);
	return 0;
}

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
	pr_info("Change force_stop setting [%d -> %d], pftch_env.is_enabled:%d from:%pS\n",
		force_stop, val, pftch_env.is_enabled,
		__builtin_return_address(0));
	// backup paranoid and set to 0 for pmu counter permission
	paranoid_backup = sysctl_perf_event_paranoid;
	sysctl_perf_event_paranoid = 0;
	if (val == true && pftch_env.is_enabled > 0) {
		ca_pmu_event_disable();
		pftch_env.is_enabled = 0;
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
	pr_info("Change force_stop setting [%d -> %d], pftch_env.is_enabled:%d\n",
			force_stop, val, pftch_env.is_enabled);
	// backup paranoid and set to 0 for pmu counter permission
	paranoid_backup = sysctl_perf_event_paranoid;
	sysctl_perf_event_paranoid = 0;
	if (val == true && pftch_env.is_enabled > 0) {
		ca_pmu_event_disable();
		pftch_env.is_enabled = 0;
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
	{ "arm,cortex-a76", ca76_register},
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
		return -ENOMEM;
	}

	init_cache_priority();
	init_cpu_config();
	for_each_possible_cpu(cpu) {

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

	register_qos_notifier(&nb);

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
			"ca_drv/l3cc:online", ca_cpu_online, ca_cpu_offline);
	if (ret < 0) {
		pr_info("%s: [Error] fail to register_cpu_notifier.\n",
				__func__);
		return ret;
	}

	ca_hp_online = ret;
	pr_info("%s: [Done]\n", __func__);

	return 0;
}

static void __exit ca_exit(void)
{
	int i, cpu;
	struct perf_event *event;

	if (!is_events_created)
		return;

	unregister_qos_notifier(&nb);
	cpuhp_remove_state_nocalls(ca_hp_online);

	mutex_lock(&ca_pmu_lock);
	if (pftch_env.is_enabled)
		ca_pmu_event_disable();
	mutex_unlock(&ca_pmu_lock);

	for_each_possible_cpu(cpu) {

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

