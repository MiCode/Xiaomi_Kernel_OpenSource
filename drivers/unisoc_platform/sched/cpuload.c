// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, Unisoc (shanghai) Technologies Co., Ltd
 *
 * /proc/cpuload implementation
 */
#include "uni_sched.h"

#if IS_ENABLED(CONFIG_SCHED_WALT)
static unsigned long uni_effective_cpu_util(int cpu, unsigned long util_cfs,
				 unsigned long max, enum cpu_util_type type,
				 struct task_struct *p)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq;
	u64 runnable_sum, walt_cpu_util;

	uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	walt_cpu_util = uni_rq->cumulative_runnable_avg;
	walt_cpu_util <<= SCHED_CAPACITY_SHIFT;
	do_div(walt_cpu_util, walt_ravg_window);

	runnable_sum = uni_rq->prev_runnable_sum > uni_rq->curr_runnable_sum ?
			uni_rq->prev_runnable_sum : uni_rq->curr_runnable_sum;

	runnable_sum <<= SCHED_CAPACITY_SHIFT;
	do_div(runnable_sum, walt_ravg_window);

	walt_cpu_util = max(walt_cpu_util, runnable_sum);

	return min_t(unsigned long, walt_cpu_util, max);
}
#else
static unsigned long uni_effective_cpu_util(int cpu, unsigned long util_cfs,
				 unsigned long max, enum cpu_util_type type,
				 struct task_struct *p)
{
	unsigned long dl_util, util, irq;
	struct rq *rq = cpu_rq(cpu);


	if (!uclamp_is_used() &&
	    type == FREQUENCY_UTIL && rt_rq_is_runnable(&rq->rt)) {
		return max;
	}

	/*
	 * Early check to see if IRQ/steal time saturates the CPU, can be
	 * because of inaccuracies in how we track these -- see
	 * update_irq_load_avg().
	 */
	irq = cpu_util_irq(rq);
	if (unlikely(irq >= max))
		return max;

	/*
	 * Because the time spend on RT/DL tasks is visible as 'lost' time to
	 * CFS tasks and we use the same metric to track the effective
	 * utilization (PELT windows are synchronized) we can directly add them
	 * to obtain the CPU's actual utilization.
	 *
	 * CFS and RT utilization can be boosted or capped, depending on
	 * utilization clamp constraints requested by currently RUNNABLE
	 * tasks.
	 * When there are no CFS RUNNABLE tasks, clamps are released and
	 * frequency will be gracefully reduced with the utilization decay.
	 */
	util = util_cfs + cpu_util_rt(rq);
	if (type == FREQUENCY_UTIL)
		util = uclamp_rq_util_with(rq, util, p);

	dl_util = cpu_util_dl(rq);

	/*
	 * For frequency selection we do not make cpu_util_dl() a permanent part
	 * of this sum because we want to use cpu_bw_dl() later on, but we need
	 * to check if the CFS+RT+DL sum is saturated (ie. no idle time) such
	 * that we select f_max when there is no idle time.
	 *
	 * NOTE: numerical errors or stop class might cause us to not quite hit
	 * saturation when we should -- something for later.
	 */
	if (util + dl_util >= max)
		return max;

	/*
	 * OTOH, for energy computation we need the estimated running time, so
	 * include util_dl and ignore dl_bw.
	 */
	if (type == ENERGY_UTIL)
		util += dl_util;

	/*
	 * There is still idle time; further improve the number by using the
	 * irq metric. Because IRQ/steal time is hidden from the task clock we
	 * need to scale the task numbers:
	 *
	 *              max - irq
	 *   U' = irq + --------- * U
	 *                 max
	 */
	util = scale_irq_capacity(util, irq, max);
	util += irq;

	/*
	 * Bandwidth required by DEADLINE must always be granted while, for
	 * FAIR and RT, we use blocked utilization of IDLE CPUs as a mechanism
	 * to gracefully reduce the frequency when no tasks show up for longer
	 * periods of time.
	 *
	 * Ideally we would like to set bw_dl as min/guaranteed freq and util +
	 * bw_dl as requested freq. However, cpufreq is not yet ready for such
	 * an interface. So, we only do the latter for now.
	 */
	if (type == FREQUENCY_UTIL)
		util += cpu_bw_dl(rq);

	return min(max, util);
}
#endif

unsigned int sched_get_cpu_util_pct(int cpu)
{
	unsigned int busy_pct;
	unsigned long cpu_util, capacity;

	capacity = arch_scale_cpu_capacity(cpu);
	cpu_util = uni_effective_cpu_util(cpu, cpu_util_cfs(cpu_rq(cpu)),
					  capacity, FREQUENCY_UTIL, NULL);

	busy_pct = div64_ul((cpu_util * 100), capacity);

	return busy_pct;
}
EXPORT_SYMBOL_GPL(sched_get_cpu_util_pct);

static int show_cpuload(struct seq_file *seq, void *v)
{
	int cpu;

	if (v == (void *)1) {
		seq_printf(seq, "timestamp %lu\n", jiffies);
		seq_printf(seq, "%-8s\t%-16s\t%-16s\t%-16s\n",
			"cpu", "cpu_load", "running_tasks", "iowait_tasks");
	} else {
		struct rq *rq;
		unsigned long cpu_util;

		cpu = (unsigned long)(v - 2);
		rq = cpu_rq(cpu);

		cpu_util = uni_effective_cpu_util(cpu, cpu_util_cfs(rq),
				arch_scale_cpu_capacity(cpu), FREQUENCY_UTIL, NULL);

		seq_printf(seq, "%-8d\t%-16lu\t%-16u\t%-16u\n",
			cpu, cpu_util, rq->nr_running, atomic_read(&rq->nr_iowait));
	}
	return 0;
}

/*
 * This iterator needs some explanation.
 * It returns 1 for the header position.
 * This means 2 is cpu 0.
 * In a hotplugged system some CPUs, including cpu 0, may be missing so we have
 * to use cpumask_* to iterate over the CPUs.
 */
static void *cpuload_start(struct seq_file *file, loff_t *offset)
{
	unsigned long n = *offset;

	if (n == 0)
		return (void *) 1;

	n--;

	if (n > 0)
		n = cpumask_next(n - 1, cpu_online_mask);
	else
		n = cpumask_first(cpu_online_mask);

	*offset = n + 1;

	if (n < nr_cpu_ids)
		return (void *)(unsigned long)(n + 2);

	return NULL;
}

static void *cpuload_next(struct seq_file *file, void *data, loff_t *offset)
{
	(*offset)++;

	return cpuload_start(file, offset);
}

static void cpuload_stop(struct seq_file *file, void *data)
{
}

static const struct seq_operations cpuload_sops = {
	.start = cpuload_start,
	.next  = cpuload_next,
	.stop  = cpuload_stop,
	.show  = show_cpuload,
};

int proc_cpuload_init(void)
{
	proc_create_seq("cpuload", 0, NULL, &cpuload_sops);
	return 0;
}
