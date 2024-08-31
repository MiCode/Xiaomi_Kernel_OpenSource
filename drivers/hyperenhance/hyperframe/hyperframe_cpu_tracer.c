// MIUI ADD: Performance_FramePredictBoost
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/preempt.h>
#include <linux/sched/cputime.h>
#include <linux/sched/task.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>
#include <uapi/linux/sched/types.h>
#include <trace/hooks/cpufreq.h>

#include "hyperframe_cpu_tracer.h"
#include "hyperframe_cpu_loading_track.h"

void cpu_frequency_tracer(void *ignore, unsigned int frequency, unsigned int cpu_id)
{
	int cpu = 0, cluster = 0;
	struct cpufreq_policy *policy = NULL;

	policy = cpufreq_cpu_get(cpu_id);
	if (!policy)
		return;

	if (cpu_id != cpumask_first(policy->related_cpus)) {
		cpufreq_cpu_put(policy);
		return;
	}

	cpufreq_cpu_put(policy);

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;
		cpu = cpumask_first(policy->related_cpus);
		if (cpu == cpu_id)
			break;
		cpu = cpumask_last(policy->related_cpus);
		cluster++;
		cpufreq_cpu_put(policy);
	}

	if (policy)
		cpufreq_cpu_put(policy);

	add_freq_data(cluster, frequency);
}

void sched_switch_tracer(void *ignore,
				bool preempt,
				struct task_struct *prev,
				 struct task_struct *next,
				 unsigned int prev_state)
{
	unsigned long long ts;
	int c_wake_cpu;
	int prev_pid;
	int next_pid;
	int cpu = 0, cluster = 0, cluster_first = 0, cluster_last = 0;
	struct cpufreq_policy *policy = NULL;

	ts = sched_clock();
	c_wake_cpu = current->wake_cpu;

	if (!prev || !next)
		return;
	prev_pid = prev->pid;
	next_pid = next->pid;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;

		cluster_first = cpumask_first(policy->related_cpus);
		cluster_last = cpumask_last(policy->related_cpus);
		if (cluster_first <= c_wake_cpu && c_wake_cpu <= cluster_last) {
			cpufreq_cpu_put(policy);
			break;
		}

		cpu = cluster_last;
		cluster++;
		cpufreq_cpu_put(policy);
	}

	add_sched_switch(cluster, prev, next);
}

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;
	FOR_EACH_INTEREST(i) {
		if (strcmp(cpu_tracepoints[i].name, tp->name) == 0)
			cpu_tracepoints[i].tp = tp;
	}
}

static void tracepoint_cleanup(void)
{
	int i;
	FOR_EACH_INTEREST(i) {
		if (cpu_tracepoints[i].registered) {
			tracepoint_probe_unregister(
				cpu_tracepoints[i].tp,
				cpu_tracepoints[i].func, NULL);
			cpu_tracepoints[i].registered = false;
		}
	}
}

static void register_cpufreq_transition_hook(void)
{
	int i, ret;
	i = 0;
	ret = 0;
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);
	FOR_EACH_INTEREST(i) {
		if (cpu_tracepoints[i].tp == NULL) {
			tracepoint_cleanup();
			return;
		}

		ret = tracepoint_probe_register(cpu_tracepoints[i].tp, cpu_tracepoints[i].func,  NULL);
		if (ret)
			return;
		cpu_tracepoints[i].registered = true;
	}
}

void hyperframe_cpu_tracer_init(void)
{
	register_cpufreq_transition_hook();
}

void hyperframe_cpu_tracer_exit(void)
{
	tracepoint_cleanup();
}
// END Performance_FramePredictBoost