// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/stat.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/threads.h>
#include <linux/rtc.h>
#include <asm/div64.h>
#include <../../../kernel/sched/sched.h>
#ifdef CONFIG_VM_EVENT_COUNTERS
#include <linux/vmstat.h>
#endif

#define NR_RECORD 3
#define HRTIMER_INTERVAL 10
#define MAX_SIZE_A_LOG	64
#define LOG_BUFF_SIZE	256
#define BUFF_ID(idx)	((idx) % (NR_RECORD))
#define NEXT_ID(idx)	BUFF_ID((idx) + 1)
#define ns_to_ms(time)	(do_div(time, NSEC_PER_MSEC))
#define ns_to_us(time)  (do_div(time, NSEC_PER_USEC))
#define unused(x) ((void)(x))

/* utime and stime defined in task_struct */
struct sprd_thread_time {
	u64	ut;
	u64	st;
};

/* thread iowait total time and cnt */
struct sprd_iowait {
	ulong	idx;
	ulong	cnt[NR_RECORD];
	u64	total[NR_RECORD];
	u64	start;
};

/* thread min_flt and maj_flt*/
struct sprd_flt {
	unsigned long sprd_min_flt;
	unsigned long sprd_maj_flt;
};

/* info kept on stack */
struct sprd_thread_info {
	struct sprd_thread_time	start;
	struct sprd_thread_time	delta[NR_RECORD];
	struct sprd_iowait io;
	struct sprd_flt sprd_start_flt;
	struct sprd_flt sprd_flt[NR_RECORD];
	ulong	idx;
};

/* cpu usage snapshot or delta */
struct sprd_cpu_stat {
	struct kernel_cpustat k;
	u64	sum;
	u64	nr_switches;
#ifdef CONFIG_VM_EVENT_COUNTERS
	unsigned long	nr_pgfault;
	unsigned long	nr_pgmajfault;
#endif
};

/* time info for a record */
struct sprd_time_info {
	u64 ns_start;
	u64 ns_end;
	struct timespec64 ts_start;
	struct timespec64 ts_end;
	struct rtc_time rtc_start;
	struct rtc_time rtc_end;
};

/* a record */
struct sprd_cpu_info {
	struct sprd_cpu_stat cpu[NR_CPUS];
	struct sprd_cpu_stat all;
	struct sprd_time_info time;
};

/* the global context */
struct sprd_cpu_usage {
	spinlock_t lock;
	struct sprd_cpu_info info[NR_RECORD];
	struct sprd_cpu_info cat;

	/* idx++ when hrtimer interval */
	unsigned long idx;
	unsigned int interval;
	struct hrtimer hrtimer;

	/* last saved */
	struct sprd_cpu_stat cpu_last[NR_CPUS];
	struct timespec64 ts_last;
	u64 ns_last;

	/* for log output */
	char *buf;
	int offset;

	/* the state of opreation */
	unsigned int cating;
	unsigned int ticking;
};
static struct sprd_cpu_usage *p_sprd_cpu_usage;

/* translation table between kernel_cpustat and our output */
static char stat_table[] = {
	CPUTIME_IDLE,
	CPUTIME_USER,
	CPUTIME_SYSTEM,
	CPUTIME_NICE,
	CPUTIME_IOWAIT,
	CPUTIME_IRQ,
	CPUTIME_SOFTIRQ,
	CPUTIME_STEAL
};

/* calculate the ratio as xx.xx% */
static void _ratio_calc(u64 dividend, u64 divider, ulong *result)
{
	u64 tmp;

	if (divider == 0) {
		result[0] = result[1] = 0;
		return;
	}

#if BITS_PER_LONG == 64
	tmp = 10000 * dividend;
	tmp = tmp / divider;
	result[1] = (ulong)(tmp % 100);
	result[0] = (ulong)(tmp / 100);
#elif BITS_PER_LONG == 32
	ns_to_us(dividend);
	ns_to_us(divider);
	tmp = 10000 * dividend;
	do_div(tmp, divider);
	result[1] = (ulong)(do_div(tmp, 100));
	result[0] = (ulong)tmp;
#endif
}

/* log to buffer temporarily */
static void sprd_cpu_log(bool new, const char *fmt, ...)
{
	va_list va;
	struct sprd_cpu_usage *p = p_sprd_cpu_usage;
	int len;

	if (!p || !p->buf)
		return;

	if (new)
		p->offset = 0;
	else if ((p->offset + MAX_SIZE_A_LOG) >= LOG_BUFF_SIZE)
		return;

	va_start(va, fmt);
	len = vsnprintf(&p->buf[p->offset], MAX_SIZE_A_LOG, fmt, va);
	va_end(va);

	p->offset += len;
}

/* io->idx catch up the global id */
static void _clean_thread_iowait(struct sprd_iowait *io, ulong id)
{
	int clear = io->idx;
	unsigned long count = id - clear;

	if (count > NR_RECORD)
		count = NR_RECORD;

	while (count--) {
		clear = NEXT_ID(clear);
		io->total[clear] = 0;
		io->cnt[clear] = 0;
	}

	io->idx = id;
}

/* t->idx catch up the global id*/
static void _clean_thread_info(struct sprd_thread_info *t, unsigned long id)
{
	int clear = t->idx;
	unsigned long count = id - clear;

	if (count > NR_RECORD)
		count = NR_RECORD;

	while (count--) {
		clear = NEXT_ID(clear);
		t->delta[clear].ut = 0;
		t->delta[clear].st = 0;
		t->sprd_flt[clear].sprd_min_flt = 0;
		t->sprd_flt[clear].sprd_maj_flt = 0;
	}

	t->idx = id;
}

#ifdef CONFIG_THREAD_INFO_IN_TASK
#define T_OFFSET (16)
#else
#define T_OFFSET (sizeof(struct thread_info) + 16)
#endif

#define T_BUF(task) ((struct sprd_thread_info *)(task->stack + T_OFFSET))

/* collect thread information: utime/stime and iowait */
void sprd_monitor_switch(struct task_struct *prev, struct task_struct *next)
{
	struct sprd_cpu_usage *p = p_sprd_cpu_usage;
	struct sprd_thread_info *t;
	struct sprd_thread_time *time;
	struct sprd_flt *flt;
	int i, idx;

	if (!p)
		return;

	/* get and keep the global idx */
	idx = p->idx;
	i = BUFF_ID(idx);

	/* update prev ut and st */
	t = T_BUF(prev);
	if (t->idx != idx)
		_clean_thread_info(t, idx);

	time = &t->delta[i];
	time->ut += (prev->utime - t->start.ut);
	time->st += (prev->stime - t->start.st);
	flt = &t->sprd_flt[i];
	flt->sprd_min_flt += (prev->min_flt - t->sprd_start_flt.sprd_min_flt);
	flt->sprd_maj_flt += (prev->maj_flt - t->sprd_start_flt.sprd_maj_flt);

	/* check if prev need start iowait */
	if (prev->in_iowait)
		t->io.start = cpu_clock(0);

	/* next keep start for st and ut */
	t = T_BUF(next);
	t->start.ut = next->utime;
	t->start.st = next->stime;
	t->sprd_start_flt.sprd_min_flt = next->min_flt;
	t->sprd_start_flt.sprd_maj_flt = next->maj_flt;

	/* check if next io wait need to update */
	if (next->in_iowait) {
		if (t->io.idx != idx)
			_clean_thread_iowait(&t->io, idx);
		t->io.total[i] += (cpu_clock(0) - t->io.start);
		t->io.cnt[i]++;
	}
}
EXPORT_SYMBOL(sprd_monitor_switch);

/* copy from fs/proc/stat.c */
#ifdef arch_idle_time
static u64 sprd_get_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle;

	idle = kcs->cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static u64 sprd_get_iowait_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 iowait;

	iowait = kcs->cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}
#else
static u64 sprd_get_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle, idle_usecs = -1ULL;

	if (cpu_online(cpu))
		idle_usecs = get_cpu_idle_time_us(cpu, NULL);

	if (idle_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcs->cpustat[CPUTIME_IDLE];
	else
		idle = idle_usecs * NSEC_PER_USEC;

	return idle;
}

static u64 sprd_get_iowait_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 iowait, iowait_usecs = -1ULL;

	if (cpu_online(cpu))
		iowait_usecs = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcs->cpustat[CPUTIME_IOWAIT];
	else
		iowait = iowait_usecs * NSEC_PER_USEC;

	return iowait;
}
#endif

/* get a snapshot of a cpu */
static void __get_cpu_stat(struct sprd_cpu_stat *stat, int cpu)
{
	struct kernel_cpustat *kcs = &kcpustat_cpu(cpu);
	int i;
#ifdef CONFIG_VM_EVENT_COUNTERS
	struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

	stat->nr_pgfault = this->event[PGFAULT];
	stat->nr_pgmajfault = this->event[PGMAJFAULT];
#endif

	stat->nr_switches = cpu_rq(cpu)->nr_switches;

	stat->sum = 0;
	for (i = 0; i < NR_STATS; i++) {
		if (i == CPUTIME_IDLE)
			stat->k.cpustat[i] = sprd_get_idle_time(kcs, cpu);
		else if (i == CPUTIME_IOWAIT)
			stat->k.cpustat[i] = sprd_get_iowait_time(kcs, cpu);
		else
			stat->k.cpustat[i] = kcs->cpustat[i];

		stat->sum += stat->k.cpustat[i];
	}
}

/* calculate a delta */
static void __get_cpu_stat_delta(struct sprd_cpu_info *info,
				 struct sprd_cpu_usage *p,
				 int c, bool update)
{
	struct sprd_cpu_stat *delta = &info->cpu[c];
	struct sprd_cpu_stat *saved = &p->cpu_last[c];
	struct sprd_cpu_stat new;
	int i;

	__get_cpu_stat(&new, c);

	for (i = 0; i < NR_STATS; i++)
		delta->k.cpustat[i] = new.k.cpustat[i] - saved->k.cpustat[i];
	delta->sum = new.sum - saved->sum;

	delta->nr_switches = new.nr_switches - saved->nr_switches;
#ifdef CONFIG_VM_EVENT_COUNTERS
	delta->nr_pgfault = new.nr_pgfault - saved->nr_pgfault;
	delta->nr_pgmajfault = new.nr_pgmajfault - saved->nr_pgmajfault;
#endif

	if (update)
		memcpy(saved, &new, sizeof(struct sprd_cpu_stat));
}


/* calculate delta for (sum of all cpu) => info->all */
static void __get_cpu_stat_delta_all(struct sprd_cpu_info *info)
{
	struct sprd_cpu_stat *all = &info->all;
	struct sprd_cpu_stat *tmp;
	int i, j;

	memset(all, 0, sizeof(struct sprd_cpu_stat));
	for_each_possible_cpu(i) {
		tmp = &info->cpu[i];

		for (j = 0; j < NR_STATS; j++)
			all->k.cpustat[j] += tmp->k.cpustat[j];
		all->sum += tmp->sum;
		all->nr_switches += tmp->nr_switches;
#ifdef CONFIG_VM_EVENT_COUNTERS
		all->nr_pgfault += tmp->nr_pgfault;
		all->nr_pgmajfault += tmp->nr_pgmajfault;
#endif
	}
}

static void __get_time_info(struct sprd_time_info *time,
			    struct sprd_cpu_usage *p, bool update)
{
	/* record the start ns/timespec64/rtc_time */
	time->ns_start = p->ns_last;
	memcpy(&time->ts_start, &p->ts_last, sizeof(struct timespec64));
	rtc_time64_to_tm(time->ts_start.tv_sec, &time->rtc_start);

	/* record the end */
	time->ns_end = cpu_clock(0);
	ktime_get_real_ts64(&time->ts_end);
	rtc_time64_to_tm(time->ts_end.tv_sec, &time->rtc_end);

	if (update) {
		p->ns_last = time->ns_end;
		memcpy(&p->ts_last, &time->ts_end, sizeof(struct timespec64));
	}
}

/*
 * get a record, contain time info and cpu usage info.
 * parameter update: true when hrtimer come, false when cat
 */
static ulong _get_a_record(struct sprd_cpu_usage *p, bool update)
{
	struct sprd_cpu_info *info;
	ulong flags, id;
	int i;

	id = p->idx;
	if (update)
		info = &p->info[BUFF_ID(id)];
	else
		info = &p->cat;

	for_each_possible_cpu(i)
		__get_cpu_stat_delta(info, p, i, update);

	__get_cpu_stat_delta_all(info);

	__get_time_info(&info->time, p, update);

	if (update) {
		spin_lock_irqsave(&p->lock, flags);
		if (p->cating)
			p->ticking++;
		else
			p->idx++;
		spin_unlock_irqrestore(&p->lock, flags);
	}

	return id;
}

/* print the time information */
static void _print_time_info(struct seq_file *m, struct sprd_cpu_info *info)
{
	struct sprd_time_info *time = &info->time;
	struct sprd_cpu_usage *p = m->private;
	struct rtc_time *rtc;
	u64 start_ms, end_ms;

	start_ms = time->ns_start;
	end_ms = time->ns_end;
	ns_to_ms(start_ms);
	ns_to_ms(end_ms);

	seq_printf(m, "Cpu Core Count: %-6d\n", num_possible_cpus());
	seq_printf(m, "Timer Circle: %-llums.\n", (end_ms - start_ms));

	rtc = &time->rtc_start;
	sprd_cpu_log(true, "  From time %llums", start_ms);
	sprd_cpu_log(false, "(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC) ",
		rtc->tm_year + 1900, rtc->tm_mon + 1, rtc->tm_mday,
		rtc->tm_hour, rtc->tm_min, rtc->tm_sec,
		time->ts_start.tv_nsec);

	sprd_cpu_log(false, "to %llums", end_ms);
	rtc = &time->rtc_end;
	sprd_cpu_log(false, "(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC).",
		rtc->tm_year + 1900, rtc->tm_mon + 1, rtc->tm_mday,
		rtc->tm_hour, rtc->tm_min, rtc->tm_sec,
		time->ts_end.tv_nsec);

	seq_printf(m, "%s\n", p->buf);
}

/* show the stat on a cpu */
static void __show_a_cpu_stat(struct seq_file *m,
			      struct sprd_cpu_stat *cpu, int c)
{
	struct sprd_cpu_usage *p = m->private;
	ulong rati[2];
	int i, j;

	if (c != -1)
		sprd_cpu_log(true, " cpu%d(%d): ", c, cpu_online(c));
	else
		sprd_cpu_log(true, " Total:   ");

	for (i = 0; i < sizeof(stat_table); i++) {
		j = stat_table[i];
		if (j >= NR_STATS)
			continue;

		if (cpu->k.cpustat[j]) {
			_ratio_calc(cpu->k.cpustat[j], cpu->sum, rati);
			sprd_cpu_log(false, "%4lu.%02lu%% ", rati[0], rati[1]);
		} else
			sprd_cpu_log(false, "%8s ", "-----");
	}

	sprd_cpu_log(false, " 100.00%% |");
	sprd_cpu_log(false, " %15llu", cpu->nr_switches);
#ifdef CONFIG_VM_EVENT_COUNTERS
	sprd_cpu_log(false, " %15llu", cpu->nr_pgfault);
	sprd_cpu_log(false, " %15llu", cpu->nr_pgmajfault);
#endif

	seq_printf(m, "%s\n", p->buf);
}

/* show all cpu stats in a record */
static void _show_cpu_stats(struct seq_file *m,
			    struct sprd_cpu_info *info, int id)
{
	struct sprd_cpu_usage *p = m->private;
	int i;

	_print_time_info(m, info);

	seq_printf(m, "%-87s   %-s\n", " * CPU USAGE:", " | * OTHER COUNTS:");
	sprd_cpu_log(true, " -%d-      %8s %8s ", id, "IDLE", "USER");
	sprd_cpu_log(false, "%8s %8s %8s ", "SYSTEM", "NICE", "IOWAIT");
	sprd_cpu_log(false, "%8s %8s %8s ", "IRQ", "SOFTIRQ", "STEAL");
	sprd_cpu_log(false, "%8s | %15s", "TOTAL", "CTXT_SWITCH");
#ifdef CONFIG_VM_EVENT_COUNTERS
	sprd_cpu_log(false, " %15s %15s", "FG_FAULT", "FG_MAJ_FAULT");
#endif
	seq_printf(m, "%s\n", p->buf);

	for_each_possible_cpu(i)
		__show_a_cpu_stat(m, &info->cpu[i], i);

	seq_puts(m, " ==================\n");
	__show_a_cpu_stat(m, &info->all, -1);
	seq_puts(m, "\n");
}

/* show a iowait: total ms + cnt */
static void __show_a_iowait(u64 ns, int cnt)
{
	if (cnt) {
		ns_to_ms(ns);
		sprd_cpu_log(false, "%5llu/%-6d|", ns, cnt);
	} else
		sprd_cpu_log(false, "%13s", "|");
}

/* show a thread: utime percent + stime percent + total percent */
static void __show_a_thread(u64 item, u64 sum)
{
	ulong ratio[2];

	if (item) {
		_ratio_calc(item, sum, ratio);
		sprd_cpu_log(false, "%4lu.%02lu%%", ratio[0], ratio[1]);
	} else
		sprd_cpu_log(false, "%8s", "-----");
}

static void _show_a_flt(ulong item1, ulong item2)
{
	if (item1)
		sprd_cpu_log(false, "%7lu/", item1);
	else
		sprd_cpu_log(false, "%7s/", "-----");

	if (item2)
		sprd_cpu_log(false, "%-7lu", item2);
	else
		sprd_cpu_log(false, "%-7s", "-----");
	sprd_cpu_log(false, "%s", "|");
}

/* thread info show */
struct _records {
	u64 time[NR_RECORD];
	u64 st[NR_RECORD];
	u64 ut[NR_RECORD];
	u64 st_all[NR_RECORD];
	u64 ut_all[NR_RECORD];
	u64 iowait[NR_RECORD];
	unsigned long sprd_min_flt[NR_RECORD];
	unsigned long sprd_maj_flt[NR_RECORD];
	int cnt[NR_RECORD];
};

/* print end with BUFF_ID(id), which means print from BUFF_ID(id+1)*/
static void _show_thread_info(struct seq_file *m, ulong id)
{
	struct sprd_cpu_usage *p = m->private;
	struct sprd_cpu_info *info;
	struct task_struct *gp, *pp;
	struct sprd_thread_info *t;
	struct sprd_thread_time	*delta;
	struct sprd_flt *sprd_flt;
	struct _records *r;
	void *stack;
	int i, j;
	bool print;

	r = kzalloc(sizeof(struct _records), GFP_KERNEL);
	if (!r)
		return;

	id = BUFF_ID(id);

	/* sequence: BUFF_ID(id+1) to id => 0 - (NR_RECORD-1) in _records */
	for (i = 0, j = NEXT_ID(id); i < NR_RECORD; i++, j = NEXT_ID(j)) {
		if (j == id)
			info = &p->cat;
		else
			info = &p->info[j];

		r->time[i] = info->time.ns_end - info->time.ns_start;
	}

	seq_puts(m, "* USAGE PER THREAD:\n");
	seq_printf(m, " %-6s%24s   %24s   %24s       %-15s  %-37s  %-s/%-8s\n", "PID", "USER",
			"SYSTEM", "TOTAL", "NAME", "IOWAIT TOTAL/CNT", "MIN_FLT", "MAJ_FLT");

	read_lock(&tasklist_lock);
	do_each_thread(gp, pp) {
		stack = try_get_task_stack(pp);
		if (stack == NULL)
			continue;

		print = false;

		t = (struct sprd_thread_info *)(stack + T_OFFSET);
		if (t->idx != p->idx)
			_clean_thread_info(t, p->idx);
		if (t->io.idx != p->idx)
			_clean_thread_iowait(&t->io, p->idx);

		for (i = 0, j = NEXT_ID(id); i < NR_RECORD; i++, j = NEXT_ID(j)) {
			delta = &t->delta[j];
			r->st[i] = delta->st;
			r->ut[i] = delta->ut;
			sprd_flt = &t->sprd_flt[j];
			r->sprd_min_flt[i] = sprd_flt->sprd_min_flt;
			r->sprd_maj_flt[i] = sprd_flt->sprd_maj_flt;

			if ((r->st[i] + r->ut[i]) != 0) {
				r->st_all[i] += r->st[i];
				r->ut_all[i] += r->ut[i];
				print = true;
			}

			r->iowait[i] = t->io.total[j];
			r->cnt[i] = t->io.cnt[j];
			if (r->cnt[i])
				print = true;
		}

		if (pp->in_iowait) {
			r->iowait[NR_RECORD - 1] += (cpu_clock(0) - t->io.start);
			r->cnt[NR_RECORD - 1]++;
			print = true;
		}

		put_task_stack(pp);

		if (!print)
			continue;

		/* thread: NR_RECORD user + NR_RECORD system + NR_RECORD total */
		sprd_cpu_log(true, " %-6d", pp->pid);
		for (i = 0; i < NR_RECORD; i++)
			__show_a_thread(r->ut[i], r->time[i]);
		sprd_cpu_log(false, "%s", " | ");
		for (i = 0; i < NR_RECORD; i++)
			__show_a_thread(r->st[i], r->time[i]);
		sprd_cpu_log(false, "%s", " | ");
		for (i = 0; i < NR_RECORD; i++)
			__show_a_thread((r->ut[i] + r->st[i]), r->time[i]);
		/* comm + NR_RECORD(iowait total / cnt) */
		sprd_cpu_log(false, "%s    %-15s:", " | ", pp->comm);
		for (i = 0; i < NR_RECORD; i++)
			__show_a_iowait(r->iowait[i], r->cnt[i]);
		for (i = 0; i < NR_RECORD; i++)
			_show_a_flt(r->sprd_min_flt[i], r->sprd_maj_flt[i]);
		seq_printf(m, "%s\n", p->buf);
	} while_each_thread(gp, pp);
	read_unlock(&tasklist_lock);

	/* print total (without iowait info ) */
	seq_puts(m, " ==================\n");
	sprd_cpu_log(true, " %-6s", "Total:");
	for (i = 0; i < NR_RECORD; i++)
		__show_a_thread(r->ut_all[i], r->time[i]);
	sprd_cpu_log(false, "%s", " | ");
	for (i = 0; i < NR_RECORD; i++)
		__show_a_thread(r->st_all[i], r->time[i]);
	sprd_cpu_log(false, "%s", " | ");
	for (i = 0; i < NR_RECORD; i++)
		__show_a_thread((r->ut_all[i] + r->st_all[i]), r->time[i]);
	sprd_cpu_log(false, "%s    %-15s", " | ", current->comm);
	seq_printf(m, "%s\n", p->buf);

	kfree(r);
}

static int show_cpu_usage(struct seq_file *m, void *v)
{
	struct sprd_cpu_usage *p = m->private;
	struct sprd_cpu_info *info;
	int cnt = NR_RECORD;
	ulong flags, id;
	int i;

	unused(v);

	if (!p)
		return 0;

	spin_lock_irqsave(&p->lock, flags);
	p->cating++;
	spin_unlock_irqrestore(&p->lock, flags);

	_get_a_record(p, false);
	id = p->idx;
	i = BUFF_ID(id);

	while (cnt--) {
		i = NEXT_ID(i);
		if (i == BUFF_ID(id))
			info = &p->cat;
		else
			info = &p->info[i];
		_show_cpu_stats(m, info, i);
	}

	_show_thread_info(m, id);

	spin_lock_irqsave(&p->lock, flags);
	p->cating--;
	if (p->cating == 0 && p->ticking) {
		p->idx += p->ticking;
		p->ticking = 0;
	}
	spin_unlock_irqrestore(&p->lock, flags);

	return 0;
}

static int cpu_usage_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_cpu_usage, (void *)p_sprd_cpu_usage);
}

static ssize_t cpu_usage_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	return len;
}

const struct file_operations cpu_usage_fops = {
	.open = cpu_usage_open,
	.read = seq_read,
	.write = cpu_usage_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static enum hrtimer_restart sprd_cpu_usage_hr_func(struct hrtimer *timer)
{
	struct sprd_cpu_usage *p;
	ktime_t kt;

	p = container_of(timer, struct sprd_cpu_usage, hrtimer);
	_get_a_record(p, true);

	kt = ms_to_ktime(p_sprd_cpu_usage->interval * MSEC_PER_SEC);
	hrtimer_forward_now(timer, kt);
	return HRTIMER_RESTART;
}

static bool sprd_cpu_usage_debugfs_init(void)
{
	struct dentry *root, *dir, *file;

	root = debugfs_lookup("sprd_debug", NULL);
	if (IS_ERR_OR_NULL(root))
		root = debugfs_create_dir("sprd_debug", NULL);
	if (IS_ERR_OR_NULL(root))
		return false;

	dir = debugfs_create_dir("cpu", root);
	if (IS_ERR_OR_NULL(dir))
		return false;

	file = debugfs_create_file("cpu_usage", 0444, dir, NULL, &cpu_usage_fops);
	if (IS_ERR_OR_NULL(file)) {
		debugfs_remove(dir);
		return false;
	}
	return true;
}

static int __init sprd_cpu_usage_init(void)
{
	struct sprd_cpu_usage *p;
	ktime_t kt;

	p = kzalloc(sizeof(struct sprd_cpu_usage), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->buf = kmalloc(LOG_BUFF_SIZE, GFP_KERNEL);
	if (!p->buf) {
		kfree(p);
		return -ENOMEM;
	}

	if (!sprd_cpu_usage_debugfs_init()) {
		kfree(p->buf);
		kfree(p);
		return -ENOMEM;
	}

	spin_lock_init(&p->lock);
	p->interval = HRTIMER_INTERVAL;
	kt = ms_to_ktime(p->interval * MSEC_PER_SEC);
	hrtimer_init(&p->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	p->hrtimer.function = sprd_cpu_usage_hr_func;
	hrtimer_start(&p->hrtimer, kt, HRTIMER_MODE_REL);

	p_sprd_cpu_usage = p;
	return 0;
}

static void __exit sprd_cpu_usage_exit(void)
{
	if (p_sprd_cpu_usage) {
		hrtimer_cancel(&p_sprd_cpu_usage->hrtimer);
		kfree(p_sprd_cpu_usage->buf);
		kfree(p_sprd_cpu_usage);
	}
}

subsys_initcall(sprd_cpu_usage_init);
module_exit(sprd_cpu_usage_exit);
