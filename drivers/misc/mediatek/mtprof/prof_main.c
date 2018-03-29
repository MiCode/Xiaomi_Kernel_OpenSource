/*
 * Copyright (C) 2015 MediaTek Inc.
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

#define DEBUG 1

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <linux/pid.h>
#include "internal.h"
#include "mt_cputime.h"

static void print_task(struct seq_file *m, struct task_struct *p)
{
	SEQ_printf(m, "%15s %5d %9Ld %5d ",
		   p->comm, p->pid, (long long)(p->nvcsw + p->nivcsw), p->prio);
#ifdef CONFIG_SCHEDSTATS
	SEQ_printf(m, "%9Ld.%06ld %9Ld.%06ld %9Ld.%06ld\n",
		   nsec_high(p->se.vruntime), nsec_low(p->se.vruntime),
		   nsec_high(p->se.sum_exec_runtime),
		   nsec_low(p->se.sum_exec_runtime),
		   nsec_high(p->se.statistics.sum_sleep_runtime),
		   nsec_low(p->se.statistics.sum_sleep_runtime));
#else
	SEQ_printf(m, "%15Ld %15Ld %15Ld.%06ld %15Ld.%06ld %15Ld.%06ld\n",
		   0LL, 0LL, 0LL, 0L, 0LL, 0L, 0LL, 0L);
#endif
}

/*========================================================================*/
/* Real work */
/*========================================================================*/
/* 1. sched info */
MT_DEBUG_ENTRY(sched_debug);
static int mt_sched_debug_show(struct seq_file *m, void *v)
{
	struct task_struct *g, *p;
	unsigned long flags;

	SEQ_printf(m, "=== mt Scheduler Profiling ===\n");
	SEQ_printf(m, "\nrunnable tasks:\n");
	SEQ_printf(m, "            task   PID   switches  prio");
	SEQ_printf(m, "     exec-runtime         sum-exec        sum-sleep\n");
	SEQ_printf(m, "------------------------------------------------------");
	SEQ_printf(m, "----------------------------------------------------\n");
	read_lock_irqsave(&tasklist_lock, flags);

	do_each_thread(g, p) {
		print_task(m, p);
	} while_each_thread(g, p);

	read_unlock_irqrestore(&tasklist_lock, flags);
	return 0;
}

static ssize_t mt_sched_debug_write(struct file *filp,
				    const char *ubuf, size_t cnt, loff_t *data)
{
	return cnt;
}

#ifdef CONFIG_MTPROF_CPUTIME
/* 2. cputime */
MT_DEBUG_ENTRY(cputime);
static int mt_cputime_show(struct seq_file *m, void *v)
{
	struct mt_proc_struct *mtproc = mt_proc_head;
	int i = 0;
	unsigned long long end_ts;
	unsigned long long total_excul_time = 0, thread_time = 0;
	unsigned long long cost_isr_time = 0;
	unsigned long long cpu_idle_time[mt_cpu_num];
	u32 div_value;
	struct task_struct *tsk;
	struct task_struct *idle;
	struct mtk_isr_info *mtk_isr = NULL;
	char status;

	if (mt_proc_head == NULL) {
		SEQ_printf(m, "Please enable cputime again!\n");
	} else {
		if (0 == prof_end_ts || mtsched_is_enabled())
			end_ts = sched_clock();
		else
			end_ts = prof_end_ts;
		prof_dur_ts = end_ts - prof_start_ts;

		for (i = 0; i < mt_cpu_num; i++) {
			if (mtsched_is_enabled() ||
			    (0 == mt_cpu_info_head[i].cpu_idletime_end)) {
				mt_cpu_info_head[i].cpu_idletime_end =
					mtprof_get_cpu_idle(i);
				if (mt_cpu_info_head[i].cpu_idletime_end <
				    mt_cpu_info_head[i].cpu_idletime_start) {
					mt_cpu_info_head[i].cpu_idletime_end =
					    mt_cpu_info_head[i].cpu_idletime_start;
				}
			}
			cpu_idle_time[i] =
			    mt_cpu_info_head[i].cpu_idletime_end -
			    mt_cpu_info_head[i].cpu_idletime_start;
			if (cpu_idle_time[i] * 1000 > prof_dur_ts) {
				cpu_idle_time[i] = prof_dur_ts;
				do_div(cpu_idle_time[i], 1000);
			}

			if (mtsched_is_enabled() ||
			    (0 == mt_cpu_info_head[i].cpu_iowait_end)) {
				mt_cpu_info_head[i].cpu_iowait_end =
					mtprof_get_cpu_iowait(i);
				if (mt_cpu_info_head[i].cpu_iowait_end <
				    mt_cpu_info_head[i].cpu_iowait_start) {
					mt_cpu_info_head[i].cpu_iowait_end =
					    mt_cpu_info_head[i].cpu_iowait_start;
				}
			}
		}

		SEQ_printf(m, "iowait time(us): %llu",
			   mt_cpu_info_head[0].cpu_iowait_end -
			   mt_cpu_info_head[0].cpu_iowait_start);
		for (i = 1; i < mt_cpu_num; i++) {
			SEQ_printf(m, " , %llu",
				   mt_cpu_info_head[i].cpu_iowait_end -
				   mt_cpu_info_head[i].cpu_iowait_start);
		}
		SEQ_printf(m, "\n");

		SEQ_printf(m, "-----------------------------------------------\n");
		SEQ_printf(m, "        Duration: %10Ld.%06ld ms\n",
			   nsec_high(end_ts - prof_start_ts), nsec_low(end_ts - prof_start_ts));
		SEQ_printf(m, "        --------------------------------\n");
		SEQ_printf(m, "           Start: %10Ld.%06ld ms\n", nsec_high(prof_start_ts), nsec_low(prof_start_ts));
		SEQ_printf(m, "             End: %10Ld.%06ld ms\n", nsec_high(end_ts), nsec_low(end_ts));
		SEQ_printf(m, "-----------------------------------------------\n");
		SEQ_printf(m,
			   "         Process:Status:   PID:  TGID:          CPUtime:       Percenta%%:          Elapsed:   User: Kernel:  ISR  type:   ISRTime:\n");

		do_div(prof_dur_ts, 1000);	/* change  to us */
		for (i = 0; i < mt_cpu_num; i++) {
			thread_time = cpu_idle_time[i] * 1000000;
			do_div(thread_time, prof_dur_ts);
			div_value = (u32) thread_time;
			idle = idle_task(i);

			SEQ_printf(m,
				   "          Idle-%1d:     L:%6d:     0:%10Ld.%03ld000:%10d.%04d%%:%10Ld.%06ld:      0:      0:%7d:%10Ld.%06ld:\n",
				   i, 0 - i, usec_high(cpu_idle_time[i]), usec_low(cpu_idle_time[i]), div_value / 10000,
				   div_value % 10000, nsec_high(end_ts - prof_start_ts),
				   nsec_low(end_ts - prof_start_ts),
				   idle->se.mtk_isr_count, nsec_high(idle->se.mtk_isr_time),
				   nsec_low(idle->se.mtk_isr_time)
			    );

			total_excul_time += (cpu_idle_time[i]) * 1000;
		}

		while (mtproc != NULL) {
			/* Record new cputime */
			tsk = find_task_by_vpid(mtproc->pid);

			if (tsk != NULL) {
				/* update cputime */
				if (mtsched_is_enabled()) {
					mtproc->cputime = tsk->se.sum_exec_runtime;	/* - tsk->se.mtk_isr_time; */
					mtproc->isr_time = tsk->se.mtk_isr_time;
					mt_task_times(tsk, &mtproc->utime, &mtproc->stime);
					mtproc->utime =
					    mtproc->utime - mtproc->utime_init;
					mtproc->stime =
					    mtproc->stime - mtproc->stime_init;
					cost_isr_time =
						mtproc->isr_time - mtproc->isr_time_init;
				}
				status = 'L';
			} else {
				status = 'D';
			}

			if (mtsched_is_enabled()) {
				if (mtproc->cputime >=
					(mtproc->cputime_init + cost_isr_time)) {
					thread_time =
					    mtproc->cputime - cost_isr_time -
							mtproc->cputime_init;
					mtproc->cost_cputime = thread_time;
					do_div(thread_time, prof_dur_ts);
					mtproc->cputime_percen_6 = thread_time;
				} else {
					mtproc->cost_cputime = 0;
					mtproc->cputime_percen_6 = 0;
				}
			}


			total_excul_time += (mtproc->cputime - mtproc->cputime_init);

			SEQ_printf(m,
				   "%16s:     %c:%6d:%6d:%10Ld.%06ld:%10d.%04d%%:%10Ld.%06ld:%7u:%7u:%7d:%10Ld.%06ld:\n",
				   mtproc->comm, status, mtproc->pid, mtproc->tgid,
				   nsec_high(mtproc->cost_cputime),
				   nsec_low(mtproc->cost_cputime),
				   mtproc->cputime_percen_6 / 10000,
				   mtproc->cputime_percen_6 % 10000,
				   nsec_high(mtproc->prof_end ==
					    0 ? end_ts - mtproc->prof_start : mtproc->prof_end -
					    mtproc->prof_start),
				   nsec_low(mtproc->prof_end ==
					    0 ? end_ts - mtproc->prof_start : mtproc->prof_end -
					    mtproc->prof_start),
				   jiffies_to_msecs(mtproc->utime),
				   jiffies_to_msecs(mtproc->stime), mtproc->isr_count,
				   nsec_high(cost_isr_time),
				   nsec_low(cost_isr_time));

			mtproc = mtproc->next;
		}

		SEQ_printf(m, "********************\n");

		for (i = 0; i < mt_cpu_num; i++) {
			idle = idle_task(i);
			mtk_isr = idle->se.mtk_isr;
			if (idle->se.mtk_isr_count != 0) {
				SEQ_printf(m,
					   "thread name:          idle-%1d, thread id: 0, total ISR type %d:\n",
					   i, idle->se.mtk_isr_count);
			}
			while (mtk_isr != NULL) {
				SEQ_printf(m,
					   "ISR name: %16s: number: %d: count: %d: total time: %10Ld.%06ld:\n",
					   mtk_isr->isr_name, mtk_isr->isr_num, mtk_isr->isr_count,
					   nsec_high(mtk_isr->isr_time),
					   nsec_low(mtk_isr->isr_time));
				mtk_isr = mtk_isr->next;
			}
		}

		mtproc = mt_proc_head;
		while (mtproc != NULL) {
			mtk_isr = mtproc->mtk_isr;
			if (mtproc->isr_count != 0) {
				SEQ_printf(m,
					   "thread name: %16s, thread id: %d, total ISR type %d:\n",
					   mtproc->comm, mtproc->pid, mtproc->isr_count);
			}
			while (mtk_isr != NULL) {
				SEQ_printf(m,
					   "ISR name: %16s: number: %d: count: %d: total time: %10Ld.%06ld:\n",
					   mtk_isr->isr_name, mtk_isr->isr_num, mtk_isr->isr_count,
					   nsec_high(mtk_isr->isr_time),
					   nsec_low(mtk_isr->isr_time));
				mtk_isr = mtk_isr->next;
			}
			mtproc = mtproc->next;
		}
		SEQ_printf(m, "********************\n");
		SEQ_printf(m, "All the thread total execult time is:%10Ld.%06ld.\n",
			   nsec_high(total_excul_time),
			   nsec_low(total_excul_time));
	}

	return 0;
}

static ssize_t mt_cputime_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	val = !!val;
	/* 0: off, 1:on */
	mt_cputime_switch(val);
	return cnt;

}
#endif
#ifdef CONFIG_MT_ENG_BUILD

MT_DEBUG_ENTRY(log);
static unsigned long print_num;
static unsigned long long second = 1;

static int mt_log_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "Print %ld lines log in %lld second in last time.\n", print_num, second);
	SEQ_printf(m, "show: Please echo m n > log again. m: second, n: level.\n");
	return 0;
}

static ssize_t mt_log_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long long t1 = 0, t2 = 0;
	int level = 0;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	if (sscanf(buf, "%lld %d ", &second, &level) == 2) {
		SEQ_printf(NULL, "will print log in level %d about %lld second.\n", level, second);
	} else {
		SEQ_printf(NULL, "Please echo m n > log; m: second, n: level.\n");
		return cnt;
	}
	t1 = sched_clock();
	pr_err("printk debug log: start time: %lld.\n", t1);
	print_num = 0;
	for (;;) {
		t2 = sched_clock();
		if (t2 - t1 > second * 1000000000)
			break;
		pr_err("printk debug log: the %ld line, time: %lld.\n", print_num++, t2);
		switch (level) {
		case 0:
			break;
		case 1:
			__delay(1);
			break;
		case 2:
			__delay(5);
			break;
		case 3:
			__delay(10);
			break;
		case 4:
			__delay(50);
			break;
		case 5:
			__delay(100);
			break;
		case 6:
			__delay(200);
			break;
		case 7:
			__delay(500);
			break;
		case 8:
			__delay(1000);
			break;
		case 9:
			msleep(20);
			break;
		default:
			msleep(20);
			break;
		}
	}

	pr_err("mt log total write %ld line in %lld second.\n", print_num, second);
	return cnt;
}
#endif

/* 6. reboot pid*/
MT_DEBUG_ENTRY(pid);

int reboot_pid = 0;
static int mt_pid_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "reboot pid %d.\n", reboot_pid);
	return 0;
}

static ssize_t mt_pid_write(struct file *filp, const char *ubuf,
	   size_t cnt, loff_t *data)
{
	char buf[10];
	unsigned long val;
	int ret;
	struct task_struct *tsk;

	if (cnt >= sizeof(buf)) {
		pr_debug("mt_pid input stream size to large.\n");
		return -EINVAL;
	}

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoul(buf, 10, &val);

	reboot_pid = val;
	if (reboot_pid > PID_MAX_DEFAULT) {
		pr_debug("get reboot pid error %d.\n", reboot_pid);
		reboot_pid = 0;
		return -EFAULT;
	}
	pr_debug("get reboot pid: %d.\n", reboot_pid);

	if (reboot_pid > 1) {
		tsk = find_task_by_vpid(reboot_pid);
		if (tsk != NULL)
			pr_crit("Reboot Process(%s:%d).\n", tsk->comm, tsk->pid);
	}

	return cnt;

}

#include <linux/signal.h>
#include <trace/events/signal.h>

#define STORE_SIGINFO(_errno, _code, info)			\
	do {							\
		if (info == SEND_SIG_NOINFO ||			\
		    info == SEND_SIG_FORCED) {			\
			_errno	= 0;				\
			_code	= SI_USER;			\
		} else if (info == SEND_SIG_PRIV) {		\
			_errno	= 0;				\
			_code	= SI_KERNEL;			\
		} else {					\
			_errno	= info->si_errno;		\
			_code	= info->si_code;		\
		}						\
	} while (0)

static const char * const signal_deliver_results[] = {
	"delivered",
	"ignored",
	"already_pending",
	"overflow_fail",
	"lost_info",
};

/* 7. signal logs */
MT_DEBUG_ENTRY(signal_log);

enum {
	SI_GENERATE = (1 << 0),
	SI_DELIVER  = (1 << 1),
} SI_LOG_MASK;

static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
static unsigned int enabled_signal_log;

static void probe_signal_generate(void *ignore, int sig, struct siginfo *info,
		struct task_struct *task, int group, int result)
{
	unsigned state = task->state ? __ffs(task->state) + 1 : 0;
	int errno, code;

	/*
	 * only log delivered signals
	 */
	if (result == TRACE_SIGNAL_DELIVERED) {
		STORE_SIGINFO(errno, code, info);
		pr_debug("[signal][%d:%s]generate sig %d to [%d:%s:%c] errno=%d code=%d grp=%d res=%s\n",
				current->pid, current->comm, sig,
				task->pid, task->comm,
				state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?',
				errno, code, group, signal_deliver_results[result]);
	}
}

static void probe_signal_deliver(void *ignore, int sig, struct siginfo *info,
		struct k_sigaction *ka)
{
	int errno, code;

	STORE_SIGINFO(errno, code, info);
	pr_debug("[signal]sig %d delivered to [%d:%s] errno=%d code=%d sa_handler=%lx sa_flags=%lx\n",
			sig, current->pid, current->comm, errno, code,
			(unsigned long)ka->sa.sa_handler, ka->sa.sa_flags);
}

static void probe_death_signal(void *ignore, int sig, struct siginfo *info,
		struct task_struct *task, int _group, int result)
{
	struct signal_struct *signal = task->signal;
	unsigned int state;
	int group;

	/*
	 * all action will cause process coredump or terminate
	 * kernel log reduction: only print delivered signals
	 */
	if ((result == TRACE_SIGNAL_DELIVERED) &&
	    sig_fatal(task, sig)) {
		signal = task->signal;
		group = _group ||
			(signal->flags & (SIGNAL_GROUP_EXIT | SIGNAL_GROUP_COREDUMP));
		/*
		 * kernel log reduction
		 * skip SIGRTMIN because it's used as timer signal
		 * skip if the target thread is already dead
		 */
		if (sig == SIGRTMIN ||
		    (task->state & (TASK_DEAD | EXIT_DEAD | EXIT_ZOMBIE)))
			return;
		/*
		 * Global init gets no signals it doesn't want.
		 * Container-init gets no signals it doesn't want from same
		 * container.
		 *
		 * Note that if global/container-init sees a sig_kernel_only()
		 * signal here, the signal must have been generated internally
		 * or must have come from an ancestor namespace. In either
		 * case, the signal cannot be dropped.
		 */
		if (unlikely(signal->flags & SIGNAL_UNKILLABLE) &&
				!sig_kernel_only(sig))
			return;
		/*
		 * kernel log reduction
		 * only print process instead of all threads
		 */
		if (group && (task != task->group_leader))
			return;

		state = task->state ? __ffs(task->state) + 1 : 0;
		pr_debug("[signal][%d:%s] send death sig %d to [%d:%s:%c]\n",
			 current->pid, current->comm,
			 sig, task->pid, task->comm,
			 state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	}
}

static int mt_signal_log_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "%d: debug message for signal being generated\n", SI_GENERATE);
	SEQ_printf(m, "%d: debug message for signal being delivered\n", SI_DELIVER);
	SEQ_printf(m, "%d: enable all logs\n", SI_GENERATE | SI_DELIVER);
	SEQ_printf(m, "%d\n", enabled_signal_log);
	return 0;
}

static ssize_t mt_signal_log_write(struct file *filp, const char *ubuf,
	   size_t cnt, loff_t *data)
{
	unsigned long val;
	unsigned long update;
	int ret;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	update = enabled_signal_log ^ val;
	if (update & SI_GENERATE) {
		if (val & SI_GENERATE)
			register_trace_signal_generate(probe_signal_generate, NULL);
		else
			unregister_trace_signal_generate(probe_signal_generate, NULL);
	}
	if (update & SI_DELIVER) {
		if (val & SI_DELIVER)
			register_trace_signal_deliver(probe_signal_deliver, NULL);
		else
			unregister_trace_signal_deliver(probe_signal_deliver, NULL);
	}
	enabled_signal_log = val;

	return cnt;
}

static void __init init_signal_log(void)
{
	if (enabled_signal_log & SI_GENERATE)
		register_trace_signal_generate(probe_signal_generate, NULL);
	if (enabled_signal_log & SI_DELIVER)
		register_trace_signal_deliver(probe_signal_deliver, NULL);
	register_trace_signal_generate(probe_death_signal, NULL);
}

/* 8. fork & exit logs */
#include <trace/events/sched.h>

MT_DEBUG_ENTRY(fork_exit_log);

enum {
	DO_FORK = (1 << 0),
	DO_EXIT  = (1 << 1),
} FORK_EXIT_LOG_MASK;

static unsigned int enabled_fork_exit_log;

static void probe_sched_fork_time(void *ignore,
	struct task_struct *parent, struct task_struct *child, unsigned long long dur)
{
	char parent_comm[TASK_COMM_LEN], child_comm[TASK_COMM_LEN];
	pid_t parent_pid, child_pid;
	unsigned long long fork_time;

	memcpy(parent_comm, parent->comm, TASK_COMM_LEN);
	parent_pid = parent->pid;
	memcpy(child_comm, child->comm, TASK_COMM_LEN);
	child_pid = child->pid;
	fork_time = dur;

	pr_debug("[fork]comm=%s pid=%d fork child_comm=%s child_pid=%d, total fork time=%llu us",
		parent_comm, parent_pid, child_comm, child_pid, fork_time);

}

static void probe_sched_process_exit(void *ignore, struct task_struct *p)
{
	char comm[TASK_COMM_LEN];
	pid_t pid;
	int prio;

	memcpy(comm, p->comm, TASK_COMM_LEN);
	pid = p->pid;
	prio = p->prio;

	pr_debug("[exit]comm=%s pid=%d prio=%d exited", comm, pid, prio);

}

static int mt_fork_exit_log_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "%d: debug message for fork\n", DO_FORK);
	SEQ_printf(m, "%d: debug message for exit\n", DO_EXIT);
	SEQ_printf(m, "%d: enable all logs\n", DO_FORK | DO_EXIT);
	SEQ_printf(m, "%d\n", enabled_fork_exit_log);
	return 0;
}

static ssize_t mt_fork_exit_log_write(struct file *filp, const char *ubuf,
	   size_t cnt, loff_t *data)
{
	unsigned long val;
	unsigned long update;
	int ret;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	update = enabled_fork_exit_log ^ val;
	if (update & DO_FORK) {
		if (val & DO_FORK)
			register_trace_sched_fork_time(probe_sched_fork_time, NULL);
		else
			unregister_trace_sched_fork_time(probe_sched_fork_time, NULL);
	}
	if (update & DO_EXIT) {
		if (val & DO_EXIT)
			register_trace_sched_process_exit(probe_sched_process_exit, NULL);
		else
			unregister_trace_sched_process_exit(probe_sched_process_exit, NULL);
	}
	enabled_fork_exit_log = val;

	return cnt;
}

static void __init init_fork_exit_log(void)
{
	if (enabled_fork_exit_log & DO_FORK)
		register_trace_sched_fork_time(probe_sched_fork_time, NULL);
	if (enabled_fork_exit_log & DO_EXIT)
		register_trace_sched_process_exit(probe_sched_process_exit, NULL);
}

/*-------------------------------------------------------------------*/
static int __init init_mtsched_prof(void)
{
	struct proc_dir_entry *pe;

	if (!proc_mkdir("mtprof", NULL))
		return -1;
	pe = proc_create("mtprof/sched", 0444, NULL, &mt_sched_debug_fops);
	if (!pe)
		return -ENOMEM;
#ifdef CONFIG_MTPROF_CPUTIME
	pe = proc_create("mtprof/cputime", 0664, NULL, &mt_cputime_fops);
	if (!pe)
		return -ENOMEM;
#endif
	pe = proc_create("mtprof/reboot_pid", 0660, NULL, &mt_pid_fops);
	if (!pe)
		return -ENOMEM;

	init_signal_log();
	pe = proc_create("mtprof/signal_log", 0664, NULL, &mt_signal_log_fops);
	if (!pe)
		return -ENOMEM;

	init_fork_exit_log();
	pe = proc_create("mtprof/fork_exit_log", 0664, NULL, &mt_fork_exit_log_fops);
	if (!pe)
		return -ENOMEM;

	mt_cpu_num = num_present_cpus();
	mt_cpu_info_head = kmalloc_array(mt_cpu_num, sizeof(struct mt_cpu_info), GFP_ATOMIC);
	if (mt_cpu_info_head == NULL)
		return -ENOMEM;
#ifdef CONFIG_MT_ENG_BUILD
	pe = proc_create("mtprof/log", 0666, NULL, &mt_log_fops);
	if (!pe)
		return -ENOMEM;
#endif
	return 0;
}

device_initcall(init_mtsched_prof);
