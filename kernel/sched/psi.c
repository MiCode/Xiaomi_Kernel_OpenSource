/*
 * Pressure stall information for CPU, memory and IO
 *
 * Copyright (c) 2018 Facebook, Inc.
 * Author: Johannes Weiner <hannes@cmpxchg.org>
 *
 * Polling support by Suren Baghdasaryan <surenb@google.com>
 * Copyright (c) 2018 Google, Inc.
 *
 * When CPU, memory and IO are contended, tasks experience delays that
 * reduce throughput and introduce latencies into the workload. Memory
 * and IO contention, in addition, can cause a full loss of forward
 * progress in which the CPU goes idle.
 *
 * This code aggregates individual task delays into resource pressure
 * metrics that indicate problems with both workload health and
 * resource utilization.
 *
 *			Model
 *
 * The time in which a task can execute on a CPU is our baseline for
 * productivity. Pressure expresses the amount of time in which this
 * potential cannot be realized due to resource contention.
 *
 * This concept of productivity has two components: the workload and
 * the CPU. To measure the impact of pressure on both, we define two
 * contention states for a resource: SOME and FULL.
 *
 * In the SOME state of a given resource, one or more tasks are
 * delayed on that resource. This affects the workload's ability to
 * perform work, but the CPU may still be executing other tasks.
 *
 * In the FULL state of a given resource, all non-idle tasks are
 * delayed on that resource such that nobody is advancing and the CPU
 * goes idle. This leaves both workload and CPU unproductive.
 *
 * (Naturally, the FULL state doesn't exist for the CPU resource.)
 *
 *	SOME = nr_delayed_tasks != 0
 *	FULL = nr_delayed_tasks != 0 && nr_running_tasks == 0
 *
 * The percentage of wallclock time spent in those compound stall
 * states gives pressure numbers between 0 and 100 for each resource,
 * where the SOME percentage indicates workload slowdowns and the FULL
 * percentage indicates reduced CPU utilization:
 *
 *	%SOME = time(SOME) / period
 *	%FULL = time(FULL) / period
 *
 *			Multiple CPUs
 *
 * The more tasks and available CPUs there are, the more work can be
 * performed concurrently. This means that the potential that can go
 * unrealized due to resource contention *also* scales with non-idle
 * tasks and CPUs.
 *
 * Consider a scenario where 257 number crunching tasks are trying to
 * run concurrently on 256 CPUs. If we simply aggregated the task
 * states, we would have to conclude a CPU SOME pressure number of
 * 100%, since *somebody* is waiting on a runqueue at all
 * times. However, that is clearly not the amount of contention the
 * workload is experiencing: only one out of 256 possible exceution
 * threads will be contended at any given time, or about 0.4%.
 *
 * Conversely, consider a scenario of 4 tasks and 4 CPUs where at any
 * given time *one* of the tasks is delayed due to a lack of memory.
 * Again, looking purely at the task state would yield a memory FULL
 * pressure number of 0%, since *somebody* is always making forward
 * progress. But again this wouldn't capture the amount of execution
 * potential lost, which is 1 out of 4 CPUs, or 25%.
 *
 * To calculate wasted potential (pressure) with multiple processors,
 * we have to base our calculation on the number of non-idle tasks in
 * conjunction with the number of available CPUs, which is the number
 * of potential execution threads. SOME becomes then the proportion of
 * delayed tasks to possibe threads, and FULL is the share of possible
 * threads that are unproductive due to delays:
 *
 *	threads = min(nr_nonidle_tasks, nr_cpus)
 *	   SOME = min(nr_delayed_tasks / threads, 1)
 *	   FULL = (threads - min(nr_running_tasks, threads)) / threads
 *
 * For the 257 number crunchers on 256 CPUs, this yields:
 *
 *	threads = min(257, 256)
 *	   SOME = min(1 / 256, 1)             = 0.4%
 *	   FULL = (256 - min(257, 256)) / 256 = 0%
 *
 * For the 1 out of 4 memory-delayed tasks, this yields:
 *
 *	threads = min(4, 4)
 *	   SOME = min(1 / 4, 1)               = 25%
 *	   FULL = (4 - min(3, 4)) / 4         = 25%
 *
 * [ Substitute nr_cpus with 1, and you can see that it's a natural
 *   extension of the single-CPU model. ]
 *
 *			Implementation
 *
 * To assess the precise time spent in each such state, we would have
 * to freeze the system on task changes and start/stop the state
 * clocks accordingly. Obviously that doesn't scale in practice.
 *
 * Because the scheduler aims to distribute the compute load evenly
 * among the available CPUs, we can track task state locally to each
 * CPU and, at much lower frequency, extrapolate the global state for
 * the cumulative stall times and the running averages.
 *
 * For each runqueue, we track:
 *
 *	   tSOME[cpu] = time(nr_delayed_tasks[cpu] != 0)
 *	   tFULL[cpu] = time(nr_delayed_tasks[cpu] && !nr_running_tasks[cpu])
 *	tNONIDLE[cpu] = time(nr_nonidle_tasks[cpu] != 0)
 *
 * and then periodically aggregate:
 *
 *	tNONIDLE = sum(tNONIDLE[i])
 *
 *	   tSOME = sum(tSOME[i] * tNONIDLE[i]) / tNONIDLE
 *	   tFULL = sum(tFULL[i] * tNONIDLE[i]) / tNONIDLE
 *
 *	   %SOME = tSOME / period
 *	   %FULL = tFULL / period
 *
 * This gives us an approximation of pressure that is practical
 * cost-wise, yet way more sensitive and accurate than periodic
 * sampling of the aggregate task states would be.
 */

#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/eventfd.h>
#include <linux/proc_fs.h>
#include <linux/seqlock.h>
#include <linux/uaccess.h>
#include <linux/cgroup.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/psi.h>
#include "sched.h"

static int psi_bug __read_mostly;

bool psi_disabled __read_mostly;
core_param(psi_disabled, psi_disabled, bool, 0644);

/* Running averages - we need to be higher-res than loadavg */
#define PSI_FREQ	(2*HZ+1UL)	/* 2 sec intervals */
#define EXP_10s		1677		/* 1/exp(2s/10s) as fixed-point */
#define EXP_60s		1981		/* 1/exp(2s/60s) */
#define EXP_300s	2034		/* 1/exp(2s/300s) */

/* PSI trigger definitions */
#define PSI_TRIG_MIN_WIN_US 500000		/* Min window size is 500ms */
#define PSI_TRIG_MAX_WIN_US 10000000	/* Max window size is 10s */
#define PSI_TRIG_UPDATES_PER_WIN 10		/* 10 updates per window */

/* Sampling frequency in nanoseconds */
static u64 psi_period __read_mostly;

/* System-level pressure and stall tracking */
static DEFINE_PER_CPU(struct psi_group_cpu, system_group_pcpu);
static struct psi_group psi_system = {
	.pcpu = &system_group_pcpu,
};

struct psi_event_info {
	u64 last_event_time;
	u64 last_event_growth;
};
/* ioctl cmd to get info of  last psi trigger event*/
#define GET_LAST_PSI_EVENT_INFO _IOR('p', 1, struct psi_event_info)

static void psi_update_work(struct work_struct *work);

static void group_init(struct psi_group *group)
{
	int cpu;

	for_each_possible_cpu(cpu)
		seqcount_init(&per_cpu_ptr(group->pcpu, cpu)->seq);
	group->avg_next_update = sched_clock() + psi_period;
	group->clock_mode = PSI_CLOCK_REGULAR;
	INIT_DELAYED_WORK(&group->clock_work, psi_update_work);
	mutex_init(&group->update_lock);
	/* Init trigger-related members */
	INIT_LIST_HEAD(&group->triggers);
	memset(group->nr_triggers, 0, sizeof(group->nr_triggers));
	group->trigger_mask = 0;
	group->trigger_min_period = U32_MAX;
	memset(group->polling_total, 0, sizeof(group->polling_total));
	group->polling_next_update = ULLONG_MAX;
	group->polling_until = 0;
}

void __init psi_init(void)
{
	if (psi_disabled)
		return;

	psi_period = jiffies_to_nsecs(PSI_FREQ);
	group_init(&psi_system);
}

static bool test_state(unsigned int *tasks, enum psi_states state)
{
	switch (state) {
	case PSI_IO_SOME:
		return tasks[NR_IOWAIT];
	case PSI_IO_FULL:
		return tasks[NR_IOWAIT] && !tasks[NR_RUNNING];
	case PSI_MEM_SOME:
		return tasks[NR_MEMSTALL];
	case PSI_MEM_FULL:
		return tasks[NR_MEMSTALL] && !tasks[NR_RUNNING];
	case PSI_CPU_SOME:
		return tasks[NR_RUNNING] > 1;
	case PSI_NONIDLE:
		return tasks[NR_IOWAIT] || tasks[NR_MEMSTALL] ||
			tasks[NR_RUNNING];
	default:
		return false;
	}
}

static void get_recent_times(struct psi_group *group, int cpu, u32 *times)
{
	struct psi_group_cpu *groupc = per_cpu_ptr(group->pcpu, cpu);
	u64 now, state_start;
	enum psi_states s;
	unsigned int seq;
	u32 state_mask;

	/* Snapshot a coherent view of the CPU state */
	do {
		seq = read_seqcount_begin(&groupc->seq);
		now = cpu_clock(cpu);
		memcpy(times, groupc->times, sizeof(groupc->times));
		state_mask = groupc->state_mask;
		state_start = groupc->state_start;
	} while (read_seqcount_retry(&groupc->seq, seq));

	/* Calculate state time deltas against the previous snapshot */
	for (s = 0; s < NR_PSI_STATES; s++) {
		u32 delta;
		/*
		 * In addition to already concluded states, we also
		 * incorporate currently active states on the CPU,
		 * since states may last for many sampling periods.
		 *
		 * This way we keep our delta sampling buckets small
		 * (u32) and our reported pressure close to what's
		 * actually happening.
		 */
		if (state_mask & (1 << s))
			times[s] += now - state_start;

		delta = times[s] - groupc->times_prev[s];
		groupc->times_prev[s] = times[s];

		times[s] = delta;
	}
}

static void calc_avgs(unsigned long avg[3], u64 time, u64 period)
{
	unsigned long pct;

	/* Sample the most recent active period */
	pct = div_u64(time * 100, period);
	pct *= FIXED_1;
	avg[0] = calc_load(avg[0], EXP_10s, pct);
	avg[1] = calc_load(avg[1], EXP_60s, pct);
	avg[2] = calc_load(avg[2], EXP_300s, pct);
}

static void collect_percpu_times(struct psi_group *group)
{
	u64 deltas[NR_PSI_STATES - 1] = { 0, };
	unsigned long nonidle_total = 0;
	int cpu;
	int s;

	/*
	 * Collect the per-cpu time buckets and average them into a
	 * single time sample that is normalized to wallclock time.
	 *
	 * For averaging, each CPU is weighted by its non-idle time in
	 * the sampling period. This eliminates artifacts from uneven
	 * loading, or even entirely idle CPUs.
	 */
	for_each_possible_cpu(cpu) {
		u32 times[NR_PSI_STATES];
		u32 nonidle;

		get_recent_times(group, cpu, times);

		nonidle = nsecs_to_jiffies(times[PSI_NONIDLE]);
		nonidle_total += nonidle;

		for (s = 0; s < PSI_NONIDLE; s++)
			deltas[s] += (u64)times[s] * nonidle;
	}

	/*
	 * Integrate the sample into the running statistics that are
	 * reported to userspace: the cumulative stall times and the
	 * decaying averages.
	 *
	 * Pressure percentages are sampled at PSI_FREQ. We might be
	 * called more often when the user polls more frequently than
	 * that; we might be called less often when there is no task
	 * activity, thus no data, and clock ticks are sporadic. The
	 * below handles both.
	 */

	/* total= */
	for (s = 0; s < NR_PSI_STATES - 1; s++)
		group->total[s] += div_u64(deltas[s], max(nonidle_total, 1UL));
}

static u64 calculate_averages(struct psi_group *group, u64 now)
{
	u64 expires, period;
	u64 avg_next_update;
	int s;

	/* avgX= */
	expires = group->avg_next_update;

	/*
	 * The periodic clock tick can get delayed for various
	 * reasons, especially on loaded systems. To avoid clock
	 * drift, we schedule the clock in fixed psi_period intervals.
	 * But the deltas we sample out of the per-cpu buckets above
	 * are based on the actual time elapsing between clock ticks.
	 */
	avg_next_update = expires + psi_period;
	period = now - group->avg_last_update;
	group->avg_last_update = now;

	for (s = 0; s < NR_PSI_STATES - 1; s++) {
		u32 sample;

		sample = group->total[s] - group->avg_total[s];
		/*
		 * Due to the lockless sampling of the time buckets,
		 * recorded time deltas can slip into the next period,
		 * which under full pressure can result in samples in
		 * excess of the period length.
		 *
		 * We don't want to report non-sensical pressures in
		 * excess of 100%, nor do we want to drop such events
		 * on the floor. Instead we punt any overage into the
		 * future until pressure subsides. By doing this we
		 * don't underreport the occurring pressure curve, we
		 * just report it delayed by one period length.
		 *
		 * The error isn't cumulative. As soon as another
		 * delta slips from a period P to P+1, by definition
		 * it frees up its time T in P.
		 */
		if (sample > period)
			sample = period;
		group->avg_total[s] += sample;
		calc_avgs(group->avg[s], sample, period);
	}

	return avg_next_update;
}

/* Trigger tracking window manupulations */
static void window_init(struct psi_window *win, u64 now, u64 value)
{
	win->start_value = value;
	win->start_time = now;
	win->per_win_growth = 0;
}

/*
 * PSI growth tracking window update and growth calculation routine.
 * This approximates a sliding tracking window by interpolating
 * partially elapsed windows using historical growth data from the
 * previous intervals. This minimizes memory requirements (by not storing
 * all the intermediate values in the previous window) and simplifies
 * the calculations. It works well because PSI signal changes only in
 * positive direction and over relatively small window sizes the growth
 * is close to linear.
 */
static u64 window_update(struct psi_window *win, u64 now, u64 value)
{
	u64 interval;
	u64 growth;

	interval = now - win->start_time;
	growth = value - win->start_value;
	/*
	 * After each tracking window passes win->start_value and
	 * win->start_time get reset and win->per_win_growth stores
	 * the average per-window growth of the previous window.
	 * win->per_win_growth is then used to interpolate additional
	 * growth from the previous window assuming it was linear.
	 */
	if (interval > win->size) {
		win->per_win_growth = growth;
		win->start_value = value;
		win->start_time = now;
	} else {
		u32 unelapsed;

		unelapsed = win->size - interval;
		growth += div_u64(win->per_win_growth * unelapsed, win->size);
	}

	return growth;
}

static u64 poll_triggers(struct psi_group *group, u64 now)
{
	struct psi_trigger *t;
	bool new_stall = false;

	/*
	 * On the first update, initialize the polling state.
	 * Keep the monitor active for at least the duration of the
	 * minimum tracking window. This prevents frequent changes to
	 * clock_mode when system bounces in and out of stall states.
	 */
	if (cmpxchg(&group->clock_mode, PSI_CLOCK_SWITCHING,
				PSI_CLOCK_POLLING) == PSI_CLOCK_SWITCHING) {
		group->polling_until = now + group->trigger_min_period *
				PSI_TRIG_UPDATES_PER_WIN;
		list_for_each_entry(t, &group->triggers, node)
			window_init(&t->win, now, group->total[t->state]);
		memcpy(group->polling_total, group->total,
				sizeof(group->polling_total));
		goto out_next;

	}

	/*
	 * On subsequent updates, calculate growth deltas and let
	 * watchers know when their specified thresholds are exceeded.
	 */
	list_for_each_entry(t, &group->triggers, node) {
		u64 growth;

		/* Check for stall activity */
		if (group->polling_total[t->state] == group->total[t->state])
			continue;

		/*
		 * Multiple triggers might be looking at the same state,
		 * remember to update group->polling_total[] once we've
		 * been through all of them. Also remember to extend the
		 * polling time if we see new stall activity.
		 */
		new_stall = true;

		/* Calculate growth since last update */
		growth = window_update(&t->win, now, group->total[t->state]);
		if (growth < t->threshold)
			continue;

		/* Limit event signaling to once per window */
		if (now < t->last_event_time + t->win.size)
			continue;

		t->last_event_time = now;
		t->last_event_growth = growth;
		/* Generate an event */
		if (cmpxchg(&t->event, 0, 1) == 0)
			wake_up_interruptible(&t->event_wait);
	}

	if (new_stall) {
		memcpy(group->polling_total, group->total,
			   sizeof(group->polling_total));
		group->polling_until = now +
			(group->trigger_min_period * PSI_TRIG_UPDATES_PER_WIN);
	}

out_next:
	/* No more new stall in the last window? Disable polling */
	if (now >= group->polling_until) {
		WARN_ONCE(group->clock_mode != PSI_CLOCK_POLLING,
				"psi: invalid clock mode %d\n",
				group->clock_mode);
		group->clock_mode = PSI_CLOCK_REGULAR;
		return ULLONG_MAX;
	}

	return now + group->trigger_min_period;
}

/*
 * Update total stall, update averages if it's time,
 * check all triggers if in polling state.
 */
static void psi_update(struct psi_group *group)
{
	u64 now;

	mutex_lock(&group->update_lock);

	collect_percpu_times(group);

	now = sched_clock();
	if (now >= group->avg_next_update)
		group->avg_next_update = calculate_averages(group, now);

	if (now >= group->polling_next_update)
		group->polling_next_update = poll_triggers(group, now);

	mutex_unlock(&group->update_lock);
}

static void psi_update_work(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct psi_group *group;
	u64 next_update;

	dwork = to_delayed_work(work);
	group = container_of(dwork, struct psi_group, clock_work);

	/*
	 * Periodically fold the per-cpu times and feed samples
	 * into the running averages.
	 */

	psi_update(group);

	/* Calculate closest update time */
	next_update = min(group->polling_next_update,
				group->avg_next_update);
	schedule_delayed_work(dwork, min(PSI_FREQ,
		nsecs_to_jiffies(next_update - sched_clock()) + 1));
}

static void record_times(struct psi_group_cpu *groupc, int cpu,
			 bool memstall_tick)
{
	u32 delta;
	u64 now;

	now = cpu_clock(cpu);
	delta = now - groupc->state_start;
	groupc->state_start = now;

	if (groupc->state_mask & (1 << PSI_IO_SOME)) {
		groupc->times[PSI_IO_SOME] += delta;
		if (groupc->state_mask & (1 << PSI_IO_FULL))
			groupc->times[PSI_IO_FULL] += delta;
	}

	if (groupc->state_mask & (1 << PSI_MEM_SOME)) {
		groupc->times[PSI_MEM_SOME] += delta;
		if (groupc->state_mask & (1 << PSI_MEM_FULL))
			groupc->times[PSI_MEM_FULL] += delta;
		else if (memstall_tick) {
			u32 sample;
			/*
			 * Since we care about lost potential, a
			 * memstall is FULL when there are no other
			 * working tasks, but also when the CPU is
			 * actively reclaiming and nothing productive
			 * could run even if it were runnable.
			 *
			 * When the timer tick sees a reclaiming CPU,
			 * regardless of runnable tasks, sample a FULL
			 * tick (or less if it hasn't been a full tick
			 * since the last state change).
			 */
			sample = min(delta, (u32)jiffies_to_nsecs(1));
			groupc->times[PSI_MEM_FULL] += sample;
		}
	}

	if (groupc->state_mask & (1 << PSI_CPU_SOME))
		groupc->times[PSI_CPU_SOME] += delta;

	if (groupc->state_mask & (1 << PSI_NONIDLE))
		groupc->times[PSI_NONIDLE] += delta;
}

static void psi_group_change(struct psi_group *group, int cpu,
			     unsigned int clear, unsigned int set)
{
	struct psi_group_cpu *groupc;
	unsigned int t, m;
	enum psi_states s;
	u32 state_mask = 0;

	groupc = per_cpu_ptr(group->pcpu, cpu);

	/*
	 * First we assess the aggregate resource states this CPU's
	 * tasks have been in since the last change, and account any
	 * SOME and FULL time these may have resulted in.
	 *
	 * Then we update the task counts according to the state
	 * change requested through the @clear and @set bits.
	 */
	write_seqcount_begin(&groupc->seq);

	record_times(groupc, cpu, false);

	for (t = 0, m = clear; m; m &= ~(1 << t), t++) {
		if (!(m & (1 << t)))
			continue;
		if (groupc->tasks[t] == 0 && !psi_bug) {
			printk_deferred(KERN_ERR "psi: task underflow! cpu=%d t=%d tasks=[%u %u %u] clear=%x set=%x\n",
					cpu, t, groupc->tasks[0],
					groupc->tasks[1], groupc->tasks[2],
					clear, set);
			psi_bug = 1;
		}
		groupc->tasks[t]--;
	}

	for (t = 0; set; set &= ~(1 << t), t++)
		if (set & (1 << t))
			groupc->tasks[t]++;

	/* Calculate state mask representing active states */
	for (s = 0; s < NR_PSI_STATES; s++) {
		if (test_state(groupc->tasks, s))
			state_mask |= (1 << s);
	}
	groupc->state_mask = state_mask;

	write_seqcount_end(&groupc->seq);

	/*
	 * If there is a trigger set on this state, make sure the
	 * clock is in polling mode, or switches over right away.
	 *
	 * Monitor state changes into PSI_CLOCK_REGULAR at the max rate
	 * of once per update window (no more than 500ms), therefore
	 * below condition should happen relatively infrequently and
	 * cpu cache invalidation rate should stay low.
	 */
	if ((state_mask & group->trigger_mask) &&
		cmpxchg(&group->clock_mode, PSI_CLOCK_REGULAR,
				PSI_CLOCK_SWITCHING) == PSI_CLOCK_REGULAR) {
		group->polling_next_update = 0;
		/* reschedule immediate update */
		cancel_delayed_work(&group->clock_work);
		schedule_delayed_work(&group->clock_work, 1);
	}
}

void psi_task_change(struct task_struct *task, int clear, int set)
{
	int cpu = task_cpu(task);

	if (!task->pid)
		return;

	if (((task->psi_flags & set) ||
	     (task->psi_flags & clear) != clear) &&
	    !psi_bug) {
		printk_deferred(KERN_ERR "psi: inconsistent task state! task=%d:%s cpu=%d psi_flags=%x clear=%x set=%x\n",
				task->pid, task->comm, cpu,
				task->psi_flags, clear, set);
		psi_bug = 1;
	}

	task->psi_flags &= ~clear;
	task->psi_flags |= set;

	psi_group_change(&psi_system, cpu, clear, set);
}

void psi_memstall_tick(struct task_struct *task, int cpu)
{
	struct psi_group_cpu *groupc;

	groupc = per_cpu_ptr(psi_system.pcpu, cpu);
	write_seqcount_begin(&groupc->seq);
	record_times(groupc, cpu, true);
	write_seqcount_end(&groupc->seq);
}

/**
 * psi_memstall_enter - mark the beginning of a memory stall section
 * @flags: flags to handle nested sections
 *
 * Marks the calling task as being stalled due to a lack of memory,
 * such as waiting for a refault or performing reclaim.
 */
void psi_memstall_enter(unsigned long *flags)
{
	struct rq *rq;

	if (psi_disabled)
		return;

	*flags = current->flags & PF_MEMSTALL;
	if (*flags)
		return;
	/*
	 * PF_MEMSTALL setting & accounting needs to be atomic wrt
	 * changes to the task's scheduling state, otherwise we can
	 * race with CPU migration.
	 */
	rq = this_rq_lock_irq();

	current->flags |= PF_MEMSTALL;
	psi_task_change(current, 0, TSK_MEMSTALL);

	raw_spin_unlock_irq(&rq->lock);
}

/**
 * psi_memstall_leave - mark the end of an memory stall section
 * @flags: flags to handle nested memdelay sections
 *
 * Marks the calling task as no longer stalled due to lack of memory.
 */
void psi_memstall_leave(unsigned long *flags)
{
	struct rq *rq;

	if (psi_disabled)
		return;

	if (*flags)
		return;
	/*
	 * PF_MEMSTALL clearing & accounting needs to be atomic wrt
	 * changes to the task's scheduling state, otherwise we could
	 * race with CPU migration.
	 */
	rq = this_rq_lock_irq();

	current->flags &= ~PF_MEMSTALL;
	psi_task_change(current, TSK_MEMSTALL, 0);

	raw_spin_unlock_irq(&rq->lock);
}

static int psi_show(struct seq_file *m, struct psi_group *group,
		    enum psi_res res)
{
	int full;

	if (psi_disabled)
		return -EOPNOTSUPP;

	psi_update(group);

	for (full = 0; full < 2 - (res == PSI_CPU); full++) {
		unsigned long avg[3];
		u64 total;
		int w;

		for (w = 0; w < 3; w++)
			avg[w] = group->avg[res * 2 + full][w];
		total = div_u64(group->total[res * 2 + full], NSEC_PER_USEC);

		seq_printf(m, "%s avg10=%lu.%02lu avg60=%lu.%02lu avg300=%lu.%02lu total=%llu\n",
			   full ? "full" : "some",
			   LOAD_INT(avg[0]), LOAD_FRAC(avg[0]),
			   LOAD_INT(avg[1]), LOAD_FRAC(avg[1]),
			   LOAD_INT(avg[2]), LOAD_FRAC(avg[2]),
			   total);
	}

	return 0;
}

static int psi_io_show(struct seq_file *m, void *v)
{
	return psi_show(m, &psi_system, PSI_IO);
}

static int psi_memory_show(struct seq_file *m, void *v)
{
	return psi_show(m, &psi_system, PSI_MEM);
}

static int psi_cpu_show(struct seq_file *m, void *v)
{
	return psi_show(m, &psi_system, PSI_CPU);
}

static int psi_io_open(struct inode *inode, struct file *file)
{
	return single_open(file, psi_io_show, NULL);
}

static int psi_memory_open(struct inode *inode, struct file *file)
{
	return single_open(file, psi_memory_show, NULL);
}

static int psi_cpu_open(struct inode *inode, struct file *file)
{
	return single_open(file, psi_cpu_show, NULL);
}

static ssize_t psi_trigger_parse(char *buf, size_t nbytes, enum psi_res res,
	enum psi_states *state, u32 *threshold_us, u32 *win_sz_us)
{
	bool some;
	bool threshold_pct;
	u32 threshold;
	u32 win_sz;
	char *p;

	p = strsep(&buf, " ");
	if (p == NULL)
		return -EINVAL;

	/* parse type */
	if (!strcmp(p, "some"))
		some = true;
	else if (!strcmp(p, "full"))
		some = false;
	else
		return -EINVAL;

	switch (res) {
	case (PSI_IO):
		*state = some ? PSI_IO_SOME : PSI_IO_FULL;
		break;
	case (PSI_MEM):
		*state = some ? PSI_MEM_SOME : PSI_MEM_FULL;
		break;
	case (PSI_CPU):
		if (!some)
			return -EINVAL;
		*state = PSI_CPU_SOME;
		break;
	default:
		return -EINVAL;
	}

	while (isspace(*buf))
		buf++;

	p = strsep(&buf, "%");
	if (p == NULL)
		return -EINVAL;

	if (buf == NULL) {
		/* % sign was not found, threshold is specified in us */
		buf = p;
		p = strsep(&buf, " ");
		if (p == NULL)
			return -EINVAL;

		threshold_pct = false;
	} else
		threshold_pct = true;

	/* parse threshold */
	if (kstrtouint(p, 0, &threshold))
		return -EINVAL;

	while (isspace(*buf))
		buf++;

	p = strsep(&buf, " ");
	if (p == NULL)
		return -EINVAL;

	/* Parse window size */
	if (kstrtouint(p, 0, &win_sz))
		return -EINVAL;

	/* Check window size */
	if (win_sz < PSI_TRIG_MIN_WIN_US || win_sz > PSI_TRIG_MAX_WIN_US)
		return -EINVAL;

	if (threshold_pct)
		threshold = (threshold * win_sz) / 100;

	/* Check threshold */
	if (threshold == 0 || threshold > win_sz)
		return -EINVAL;

	*threshold_us = threshold;
	*win_sz_us = win_sz;

	return 0;
}

static struct psi_trigger *psi_trigger_create(struct psi_group *group,
		enum psi_states state, u32 threshold_us, u32 win_sz_us)
{
	struct psi_trigger *t;

	if (psi_disabled)
		return ERR_PTR(-EOPNOTSUPP);

	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (!t)
		return ERR_PTR(-ENOMEM);

	t->group = group;
	t->state = state;
	t->threshold = threshold_us * NSEC_PER_USEC;
	t->win.size = win_sz_us * NSEC_PER_USEC;
	t->event = 0;
	init_waitqueue_head(&t->event_wait);
	kref_init(&t->refcount);

	mutex_lock(&group->update_lock);

	list_add(&t->node, &group->triggers);
	group->trigger_min_period = min(group->trigger_min_period,
		t->win.size / PSI_TRIG_UPDATES_PER_WIN);
	group->nr_triggers[t->state]++;
	group->trigger_mask |= (1 << t->state);

	mutex_unlock(&group->update_lock);

	return t;
}

static void psi_trigger_destroy(struct kref *ref)
{
	struct psi_trigger *t = container_of(ref, struct psi_trigger, refcount);
	struct psi_group *group = t->group;

	if (psi_disabled)
		return;

	/*
	 * Wakeup waiters to stop polling.
	 * Can happen if cgroup is deleted from under
	 * a polling process.
	 */
	wake_up_interruptible(&t->event_wait);

	mutex_lock(&group->update_lock);
	if (!list_empty(&t->node)) {
		struct psi_trigger *tmp;
		u64 period = ULLONG_MAX;

		list_del_init(&t->node);
		group->nr_triggers[t->state]--;
		if (!group->nr_triggers[t->state])
			group->trigger_mask &= ~(1 << t->state);
		/* reset min update period for the remaining triggers */
		list_for_each_entry(tmp, &group->triggers, node) {
			period = min(period,
				tmp->win.size / PSI_TRIG_UPDATES_PER_WIN);
		}
		group->trigger_min_period = period;
	}
	mutex_unlock(&group->update_lock);

	/*
	 * Wait for *trigger_ptr from psi_trigger_replace to complete their read-side critical sections
	 * before destroying the trigger
	 */
	synchronize_rcu();
	kfree(t);
}

void psi_trigger_replace(void **trigger_ptr, struct psi_trigger *new)
{
	struct psi_trigger *old = *trigger_ptr;

	if (psi_disabled)
		return;

	rcu_assign_pointer(*trigger_ptr, new);
	if (old)
		kref_put(&old->refcount, psi_trigger_destroy);
}

static unsigned int psi_trigger_poll(void **trigger_ptr, struct file *file,
			      poll_table *wait)
{
	unsigned int ret = DEFAULT_POLLMASK;
	struct psi_trigger *t;

	if (psi_disabled)
		return DEFAULT_POLLMASK | POLLERR | POLLPRI;

	rcu_read_lock();
	t = rcu_dereference(*(void __rcu __force **)trigger_ptr);
	if (!t) {
		rcu_read_unlock();
		return DEFAULT_POLLMASK | POLLERR | POLLPRI;
	}
	kref_get(&t->refcount);

	rcu_read_unlock();

	poll_wait(file, &t->event_wait, wait);

	if (cmpxchg(&t->event, 1, 0) == 1)
		ret |= POLLPRI;

	kref_put(&t->refcount, psi_trigger_destroy);

	return ret;
}

static ssize_t psi_write(struct file *file, const char __user *user_buf,
				size_t nbytes, enum psi_res res)
{
	char buf[32];
	size_t buf_size;
	struct seq_file *seq;
	struct psi_trigger *new;
	enum psi_states state;
	u32 threshold_us;
	u32 win_sz_us;
	ssize_t ret;

	if (psi_disabled)
		return -EOPNOTSUPP;

	buf_size = min(nbytes, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size - 1] = '\0';

	ret = psi_trigger_parse(buf, nbytes, res,
				&state, &threshold_us, &win_sz_us);
	if (ret < 0)
		return ret;

	new = psi_trigger_create(&psi_system,
					state, threshold_us, win_sz_us);
	if (IS_ERR(new))
		return PTR_ERR(new);

	seq = file->private_data;
	/* Take seq->lock to protect seq->private from concurrent writes */
	mutex_lock(&seq->lock);
	psi_trigger_replace(&seq->private, new);
	mutex_unlock(&seq->lock);

	return nbytes;
}

static ssize_t psi_io_write(struct file *file,
		const char __user *user_buf, size_t nbytes, loff_t *ppos)
{
	return psi_write(file, user_buf, nbytes, PSI_IO);
}

static ssize_t psi_memory_write(struct file *file,
		const char __user *user_buf, size_t nbytes, loff_t *ppos)
{
	return psi_write(file, user_buf, nbytes, PSI_MEM);
}

static ssize_t psi_cpu_write(struct file *file,
		const char __user *user_buf, size_t nbytes, loff_t *ppos)
{
	return psi_write(file, user_buf, nbytes, PSI_CPU);
}

static unsigned int psi_fop_poll(struct file *file, poll_table *wait)
{
	struct seq_file *seq = file->private_data;

	return psi_trigger_poll(&seq->private, file, wait);
}

static long psi_fop_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct seq_file *seq = file->private_data;
	void **trigger_ptr = &seq->private;
	struct psi_trigger *t;
	unsigned int ret;
	void __user *ubuf = (void __user *)arg;

	if (psi_disabled)
		return -EOPNOTSUPP;

	switch (cmd) {
	case GET_LAST_PSI_EVENT_INFO: {
		struct psi_event_info last_event_info;
		rcu_read_lock();
		t = rcu_dereference(*(void __rcu __force **)trigger_ptr);
		if (t) {
			last_event_info.last_event_time = t->last_event_time;
			last_event_info.last_event_growth = t->last_event_growth;
			ret = 0;
		} else {
			ret = -EFAULT;
		}
		rcu_read_unlock();
		if (!ret) {
			if (copy_to_user(ubuf, &last_event_info, sizeof(last_event_info))) {
				ret = -EFAULT;
			}
		}
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int psi_fop_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;

	psi_trigger_replace(&seq->private, NULL);
	return single_release(inode, file);
}

static const struct file_operations psi_io_fops = {
	.open           = psi_io_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.write          = psi_io_write,
	.poll           = psi_fop_poll,
	.release        = psi_fop_release,
	.unlocked_ioctl = psi_fop_ioctl,
	.compat_ioctl   = psi_fop_ioctl,
};

static const struct file_operations psi_memory_fops = {
	.open           = psi_memory_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.write          = psi_memory_write,
	.poll           = psi_fop_poll,
	.release        = psi_fop_release,
	.unlocked_ioctl = psi_fop_ioctl,
	.compat_ioctl   = psi_fop_ioctl,
};

static const struct file_operations psi_cpu_fops = {
	.open           = psi_cpu_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.write          = psi_cpu_write,
	.poll           = psi_fop_poll,
	.release        = psi_fop_release,
	.unlocked_ioctl = psi_fop_ioctl,
	.compat_ioctl   = psi_fop_ioctl,
};

static int __init psi_late_init(void)
{
	if (psi_disabled)
		return 0;

	schedule_delayed_work(&psi_system.clock_work, PSI_FREQ);

	proc_mkdir("pressure", NULL);
	proc_create("pressure/io", 0, NULL, &psi_io_fops);
	proc_create("pressure/memory", 0, NULL, &psi_memory_fops);
	proc_create("pressure/cpu", 0, NULL, &psi_cpu_fops);

	return 0;
}
module_init(psi_late_init);
