#ifdef CONFIG_MT_LOAD_BALANCE_PROFILER

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <mtlbprof/mtlbprof.h>
#include <linux/version.h>

#define MT_LBPROF_VERSION 4

/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000000);
		return -nsec;
	}
	do_div(nsec, 1000000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000000);
}

#define SPLIT_NS(x) nsec_high(x), nsec_low(x)

/* ---------------------------------------------------------- */
#define SPLIT_PERCENT(x) ((x)/100), ((x)%100)
/* ---------------------------------------------------------- */

void mt_lbprof_rqinfo(char *strings)
{
	char msg2[5];
	int i;
	for_each_possible_cpu(i) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
		snprintf(msg2, 4, "%lu:", cpu_rq(i)->nr_running);
#else
		snprintf(msg2, 4, "%u:", cpu_rq(i)->nr_running);
#endif
		strcat(strings, msg2);
	}
}

/* ---------------------------------------------------------- */
/* http://git.kernel.org/?p=linux/kernel/git/stable/linux-stable.git;a=commitdiff;h=6beea0cda8ce71c01354e688e5735c47e331e84f */
static unsigned long long mtprof_get_cpu_idle(int cpu)
{
	unsigned long long *unused = 0, wall;
	unsigned long long idle_time = get_cpu_idle_time_us(cpu, unused);
	idle_time += get_cpu_iowait_time_us(cpu, &wall);
	return idle_time;
}

/* ---------------------------------------------------------- */
DEFINE_SPINLOCK(mt_lbprof_check_cpu_idle_spinlock);
EXPORT_SYMBOL(mt_lbprof_check_cpu_idle_spinlock);

static int mt_lbprof_start;
unsigned int start_output;
unsigned long long last_update;
unsigned long long start;
static DEFINE_PER_CPU(unsigned long long, start_idle_time);
static DEFINE_PER_CPU(int, lb_state);

static unsigned long time_slice_ns[2][3] = { {0, 0, 0}, {0, 0, 0} };

static unsigned long unbalance_slice_ns;
static unsigned long long period_time;
/* --------------------------------------------------------- */
int mt_lbprof_enable(void)
{
	int i, j;
	int cpu;
	unsigned long irq_flags;
	char strings[128] = "";

	spin_lock_irqsave(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);
	mt_lbprof_start = 1;
	start_output = 0;

	last_update = local_clock();
	start = last_update;

	for_each_present_cpu(cpu) {
		per_cpu(start_idle_time, cpu) = mtprof_get_cpu_idle(cpu);
		per_cpu(lb_state, cpu) = 0;
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 3; j++) {
			time_slice_ns[i][j] = 0;
		}
	}

	unbalance_slice_ns = 0;
	period_time = 0;

	spin_unlock_irqrestore(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);

	snprintf(strings, 128, "enable mtk load balance profiler");
	trace_sched_lbprof_update(strings);

	return 0;
}
late_initcall(mt_lbprof_enable);

int mt_lbprof_disable(void)
{
	unsigned long irq_flags;
	char strings[128] = "";

	spin_lock_irqsave(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);
	mt_lbprof_start = 0;
	spin_unlock_irqrestore(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);

	snprintf(strings, 128, "disable mtk load balance profiler");
	trace_sched_lbprof_update(strings);

	return 0;
}

/* ---------------------------------------------------------- */
#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif

void mt_lbprof_update_state_has_lock(int cpu, int rq_cnt)
{
	int unbalance = 0;
	unsigned long long now, delta;
	char pre_state[40] = "", post_state[40] = "", tmp_state[5] = "";
	int state[2];
	int cpu1, cpu2, tmp_cpu;
	struct cpumask cpu_mask;
	unsigned long irq_flags;

	if (!mt_lbprof_start)
		return;

	spin_lock_irqsave(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);

	if ((MT_LBPROF_BALANCE_FAIL_STATE == rq_cnt) &&
	    (per_cpu(lb_state, cpu) == MT_LBPROF_HOTPLUG_STATE)) {
		spin_unlock_irqrestore(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);
		return;
	}

	if (per_cpu(lb_state, cpu) == rq_cnt) {
		spin_unlock_irqrestore(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);
		return;
	}

	now = local_clock();

	delta = now - start;

	if (delta > 0) {
		cpumask_copy(&cpu_mask, cpu_possible_mask);
		for_each_cpu(cpu1, cpu_possible_mask) {
			cpumask_clear_cpu(cpu1, &cpu_mask);
			state[0] = per_cpu(lb_state, cpu1);
			if (state[0] < 5) {
				for_each_cpu(cpu2, &cpu_mask) {
					state[1] = per_cpu(lb_state, cpu2);
					if (state[0] < 2 && (state[1] < 5 && state[1] >= 2)) {
						unbalance = 1;
						time_slice_ns[state[0]][state[1] - 2] += delta;
					} else if (state[1] < 2 && state[0] >= 2) {
						unbalance = 1;
						time_slice_ns[state[1]][state[0] - 2] += delta;
					}
				}
			}
		}
		if (unbalance)
			unbalance_slice_ns += delta;
		period_time += delta;
	}

	for_each_cpu(tmp_cpu, cpu_possible_mask) {
		snprintf(tmp_state, 5, "%d ", per_cpu(lb_state, tmp_cpu));
		strncat(pre_state, tmp_state, 3);
		if (tmp_cpu == cpu && rq_cnt != MT_LBPROF_UPDATE_STATE) {
			if ((per_cpu(lb_state, cpu) == MT_LBPROF_ALLOW_UNBLANCE_STATE) &&
			    ((rq_cnt == MT_LBPROF_IDLE_STATE)
			     || (rq_cnt == MT_LBPROF_NO_TASK_STATE))) {
				continue;
			} else {
				per_cpu(lb_state, tmp_cpu) = rq_cnt;
			}

			continue;
		}

		if ((tmp_cpu != cpu)
		    && (per_cpu(lb_state, tmp_cpu) == MT_LBPROF_ALLOW_UNBLANCE_STATE))
			continue;

		if (0 == cpu_online(tmp_cpu)) {
			per_cpu(lb_state, tmp_cpu) = MT_LBPROF_HOTPLUG_STATE;
		}

		switch (cpu_rq(tmp_cpu)->nr_running) {
		case 1:
			per_cpu(lb_state, tmp_cpu) = MT_LBPROF_ONE_TASK_STATE;
			break;
		case 0:
			if ((per_cpu(lb_state, tmp_cpu) == MT_LBPROF_HOTPLUG_STATE)
			    || (per_cpu(lb_state, tmp_cpu) == MT_LBPROF_IDLE_STATE)
			    || (per_cpu(lb_state, tmp_cpu) == MT_LBPROF_NO_TASK_STATE)) {
				break;
			}
			per_cpu(lb_state, tmp_cpu) = MT_LBPROF_NO_TASK_STATE;
			break;
		default:
			if ((per_cpu(lb_state, tmp_cpu) == MT_LBPROF_N_TASK_STATE)
			    || (per_cpu(lb_state, tmp_cpu) == MT_LBPROF_AFFINITY_STATE)
			    || (per_cpu(lb_state, tmp_cpu) == MT_LBPROF_FAILURE_STATE)
			    || (per_cpu(lb_state, tmp_cpu) == MT_LBPROF_BALANCE_FAIL_STATE)
			    || (per_cpu(lb_state, tmp_cpu) == MT_LBPROF_ALLPINNED)) {
				break;
			}
			per_cpu(lb_state, tmp_cpu) = MT_LBPROF_N_TASK_STATE;
		}
	}

	if (delta >= 1000000 && unbalance) {
		char strings[128] = "";

		if (0 == start_output) {
			snprintf(strings, 128, "%llu.%06lu 0 %s", SPLIT_NS(start), pre_state);
			trace_sched_lbprof_update(strings);
		}

		for_each_cpu(tmp_cpu, cpu_possible_mask) {
			snprintf(tmp_state, 5, "%d ", per_cpu(lb_state, tmp_cpu));
			strncat(post_state, tmp_state, 3);
		}

		snprintf(strings, 128, "%llu.%06lu %llu %s%lu %d %d ", SPLIT_NS(now), delta,
			 post_state, unbalance_slice_ns, cpu, rq_cnt);
		mt_lbprof_rqinfo(strings);
		trace_sched_lbprof_update(strings);
		start_output = 1;
	} else {
		start_output = 0;
	}

	start = now;

	spin_unlock_irqrestore(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);

	return;
}

void mt_lbprof_update_state(int cpu, int rq_cnt)
{
	struct rq *rq;
	unsigned long irq_flags;

	rq = cpu_rq(cpu);
	raw_spin_lock_irqsave(&rq->lock, irq_flags);
	mt_lbprof_update_state_has_lock(cpu, rq_cnt);
	raw_spin_unlock_irqrestore(&rq->lock, irq_flags);
}

#define TRIMz(x)  ((tz = (unsigned long long)(x)) < 0 ? 0 : tz)
void mt_lbprof_update_status(void)
{
	int cpu, i, j;
	unsigned long long now, delta;
	unsigned long irq_flags;
	unsigned long long end_idle_time = 0;
	unsigned long lb_idle_time = 0;
	unsigned long cpu_load, period_time_32;
	char cpu_load_info[80] = "", cpu_load_info_tmp[8];

	if (!mt_lbprof_start)
		return;

	spin_lock_irqsave(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);
	now = local_clock();
	delta = now - last_update;

	if (delta < 1000000000) {
		spin_unlock_irqrestore(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);
		return;
	}

	last_update = now;
	spin_unlock_irqrestore(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);

	cpu = smp_processor_id();
	mt_lbprof_update_state(cpu, MT_LBPROF_UPDATE_STATE);

	spin_lock_irqsave(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);
	do_div(period_time, 1000);
	period_time_32 = period_time;

	for_each_present_cpu(cpu) {
		end_idle_time = mtprof_get_cpu_idle(cpu);
		lb_idle_time = end_idle_time - per_cpu(start_idle_time, cpu);
		per_cpu(start_idle_time, cpu) = end_idle_time;
		cpu_load = 10000 - lb_idle_time * 2500 / (period_time_32 / 4);
		snprintf(cpu_load_info_tmp, 8, "%3lu.%02lu ", SPLIT_PERCENT(cpu_load));
		strcat(cpu_load_info, cpu_load_info_tmp);
	}

	period_time_32 /= 10;
	{
		char strings[128] = "";
		snprintf(strings, 128,
			 "%3lu.%02lu %s%3lu.%02lu %3lu.%02lu %3lu.%02lu %3lu.%02lu %3lu.%02lu %3lu.%02lu %3lu.%02lu %lu ",
			 SPLIT_PERCENT(10000 - (unbalance_slice_ns) / period_time_32),
			 cpu_load_info, SPLIT_PERCENT(unbalance_slice_ns / period_time_32),
			 SPLIT_PERCENT(time_slice_ns[0][0] / period_time_32),
			 SPLIT_PERCENT(time_slice_ns[0][1] / period_time_32),
			 SPLIT_PERCENT(time_slice_ns[0][2] / period_time_32),
			 SPLIT_PERCENT(time_slice_ns[1][0] / period_time_32),
			 SPLIT_PERCENT(time_slice_ns[1][1] / period_time_32),
			 SPLIT_PERCENT(time_slice_ns[1][2] / period_time_32), period_time_32);
		trace_sched_lbprof_status(strings);
	}
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 3; j++) {
			time_slice_ns[i][j] = 0;
		}
	}
	unbalance_slice_ns = 0;
	period_time = 0;
	spin_unlock_irqrestore(&mt_lbprof_check_cpu_idle_spinlock, irq_flags);
}

#else				/* CONFIG_MT_LOAD_BALANCE_PROFILER */
int mt_lbprof_enable(void)
{
	return 0;
}

int mt_lbprof_disable(void)
{
	return 0;
}

void mt_lbprof_update_status(void)
{
}

void mt_lbprof_update_state(int cpu, int rq_cnt)
{
}

void mt_lbprof_update_state_has_lock(int cpu, int rq_cnt)
{
}

#endif				/* CONFIG_MT_LOAD_BALANCE_PROFILER */
