#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/mt_wq_debug.h>
#include <linux/xlog.h>
#include "prof_ctl.h"


#include <linux/pid.h>
#define SEQ_printf(m, x...)	    \
 do {			    \
    if (m)		    \
	seq_printf(m, x);	\
    else		    \
	pr_err(x);	    \
 } while (0)

#define MT_DEBUG_ENTRY(name) \
static int mt_##name##_show(struct seq_file *m, void *v);\
static ssize_t mt_##name##_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data);\
static int mt_##name##_open(struct inode *inode, struct file *file) \
{ \
    return single_open(file, mt_##name##_show, inode->i_private); \
} \
\
static const struct file_operations mt_##name##_fops = { \
    .open = mt_##name##_open, \
    .write = mt_##name##_write,\
    .read = seq_read, \
    .llseek = seq_lseek, \
    .release = single_release, \
};\
void mt_##name##_switch(int on);


#if defined(CONFIG_MTPROF_CPUTIME) || defined(CONFIG_SCHEDSTATS)
/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

#define SPLIT_NS(x) nsec_high(x), nsec_low(x)
#endif

#ifdef CONFIG_MTPROF_CPUTIME
static long long usec_high(unsigned long long usec)
{
	if ((long long)usec < 0) {
		usec = -usec;
		do_div(usec, 1000);
		return -usec;
	}
	do_div(usec, 1000);

	return usec;
}

static unsigned long usec_low(unsigned long long usec)
{
	if ((long long)usec < 0)
		usec = -usec;

	return do_div(usec, 1000);
}

#define SPLIT_US(x) usec_high(x), usec_low(x)

#endif

static void print_task(struct seq_file *m, struct task_struct *p)
{
	SEQ_printf(m, "%15s %5d %9Ld %5d ",
		   p->comm, p->pid, (long long)(p->nvcsw + p->nivcsw), p->prio);
#ifdef CONFIG_SCHEDSTATS
	SEQ_printf(m, "%9Ld.%06ld %9Ld.%06ld %9Ld.%06ld\n",
		   SPLIT_NS(p->se.vruntime),
		   SPLIT_NS(p->se.sum_exec_runtime), SPLIT_NS(p->se.statistics.sum_sleep_runtime));
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
	SEQ_printf(m,
		   "\nrunnable tasks:\n"
		   "            task   PID   switches  prio"
		   "     exec-runtime         sum-exec        sum-sleep\n"
		   "------------------------------------------------------"
		   "----------------------------------------------------\n");
	read_lock_irqsave(&tasklist_lock, flags);

	do_each_thread(g, p) {
		print_task(m, p);
	} while_each_thread(g, p);

	read_unlock_irqrestore(&tasklist_lock, flags);
	return 0;
}

static ssize_t mt_sched_debug_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	return cnt;
}

/* 2. cputime */
MT_DEBUG_ENTRY(cputime);
static int mt_cputime_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_MTPROF_CPUTIME
	struct mt_proc_struct *mtproc = mt_proc_head;
	int i = 0;
	unsigned long long end_ts;
	unsigned long long total_excul_time = 0, thread_time = 0;
	unsigned long long cpu_idle_time[mt_cpu_num];
	u32 div_value;
	struct task_struct *tsk;
	struct task_struct *idle;
	struct mtk_isr_info *mtk_isr = NULL;
	char status;

	if (mt_proc_head == NULL) {
		SEQ_printf(m, "Please enable cputime again!\n");
	} else {
		if (0 == prof_end_ts || mtsched_enabled) {
			end_ts = sched_clock();
		} else {
			end_ts = prof_end_ts;
		}
		prof_dur_ts = end_ts - prof_start_ts;

		for (i = 0; i < mt_cpu_num; i++) {
			if (mtsched_enabled || (0 == mt_cpu_info_head[i].cpu_idletime_end)) {
				mt_cpu_info_head[i].cpu_idletime_end = mtprof_get_cpu_idle(i);
				if (mt_cpu_info_head[i].cpu_idletime_end <
				    mt_cpu_info_head[i].cpu_idletime_start) {
					pr_err("the %d cpu idletime end is %10Ld.%10ld\n", i,
					       SPLIT_US(mt_cpu_info_head[i].cpu_idletime_end));
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

			if (mtsched_enabled || (0 == mt_cpu_info_head[i].cpu_iowait_end)) {
				mt_cpu_info_head[i].cpu_iowait_end = mtprof_get_cpu_iowait(i);
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
			   SPLIT_NS(end_ts - prof_start_ts));
		SEQ_printf(m, "        --------------------------------\n");
		SEQ_printf(m, "           Start: %10Ld.%06ld ms\n", SPLIT_NS(prof_start_ts));
		SEQ_printf(m, "             End: %10Ld.%06ld ms\n", SPLIT_NS(end_ts));
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
				   i, 0 - i, SPLIT_US(cpu_idle_time[i]), div_value / 10000,
				   div_value % 10000, SPLIT_NS(end_ts - prof_start_ts),
				   idle->se.mtk_isr_count, SPLIT_NS(idle->se.mtk_isr_time)
			    );

			total_excul_time += (cpu_idle_time[i]) * 1000;
		}

		while (mtproc != NULL) {
			/* Record new cputime */
			tsk = find_task_by_vpid(mtproc->pid);

			if (tsk != NULL) {
				/* update cputime */
				if (mtsched_enabled) {
					mtproc->cputime = tsk->se.sum_exec_runtime;	/* - tsk->se.mtk_isr_time; */
					mtproc->isr_time = tsk->se.mtk_isr_time;
					mt_task_times(tsk, &mtproc->utime, &mtproc->stime);
					mtproc->utime =
					    cputime_sub(mtproc->utime, mtproc->utime_init);
					mtproc->stime =
					    cputime_sub(mtproc->stime, mtproc->stime_init);
				}
				status = 'L';
			} else {
				status = 'D';
			}

			if (mtsched_enabled) {
				if (mtproc->cputime >= (mtproc->cputime_init + mtproc->isr_time)) {
					thread_time =
					    cputime_sub(mtproc->cputime - mtproc->isr_time,
							mtproc->cputime_init);
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
				   SPLIT_NS(mtproc->cost_cputime), mtproc->cputime_percen_6 / 10000,
				   mtproc->cputime_percen_6 % 10000,
				   SPLIT_NS(mtproc->prof_end ==
					    0 ? end_ts - mtproc->prof_start : mtproc->prof_end -
					    mtproc->prof_start), cputime_to_msecs(mtproc->utime),
				   cputime_to_msecs(mtproc->stime), mtproc->isr_count,
				   SPLIT_NS(mtproc->isr_time));

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
					   SPLIT_NS(mtk_isr->isr_time));
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
					   SPLIT_NS(mtk_isr->isr_time));
				mtk_isr = mtk_isr->next;
			}
			mtproc = mtproc->next;
		}
		SEQ_printf(m, "********************\n");
		SEQ_printf(m, "All the thread total execult time is:%10Ld.%06ld.\n",
			   SPLIT_NS(total_excul_time));
	}

#endif
	return 0;
}

static ssize_t mt_cputime_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
#ifdef CONFIG_MTPROF_CPUTIME
	char buf[64];
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	pr_err("mtsched_proc input count %zd.\n", cnt);

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	pr_err("mtsched_proc input stream:%s\n", buf);
/* val = !!val; */
	/* 0: off, 1:on */
	mt_cputime_switch(val);
#endif
	return cnt;

}

/* 4. prof status*/
MT_DEBUG_ENTRY(status);
#define MT_CPUTIME 1
unsigned long mtprof_status = 0;
static int mt_status_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "%lu\n", mtprof_status);
	return 0;
}

static ssize_t mt_status_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;
	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	/* 0: off, 1:on */
	pr_err("[mtprof] status = 0x%x\n", (unsigned int)mtprof_status);
	mtprof_status = val;
	pr_err("[mtprof] new status = 0x%x\n", (unsigned int)mtprof_status);
	return cnt;
}

/* 5. workqueue debug */
#ifdef CONFIG_MTK_WQ_DEBUG

extern int mt_dump_wq_debugger(void);
int wq_tracing = 0;
int wq_debugger_enable = 1;

MT_DEBUG_ENTRY(wq_dump);
static int mt_wq_dump_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t mt_wq_dump_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;
	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	mt_dump_wq_debugger();

	return cnt;
}

static void print_help(struct seq_file *m)
{
#define WQ_LOG_TAG "wq_debug"

	if (m != NULL) {
		SEQ_printf(m, "\n*** Usage ***\n");
		SEQ_printf(m, "commands to enable logs\n");
		SEQ_printf(m,
			   "  echo [queue work] [activate work] [execute work] [wq debugger] > wq_enable_logs\n");
		SEQ_printf(m, "  ex. echo 1 1 1 1  > wq_enable_logs, to enable all logs\n");
		SEQ_printf(m,
			   "  ex. echo 1 0 0 1  > wq_enable_logs, to enable \"queue work\" & \"wq debug\" logs\n");
		SEQ_printf(m,
			   "  Note, please try to enable [wq debugger] as possible as you can.\n");
	} else {
		pr_err("\n*** Usage ***\n");
		pr_err("commands to enable logs\n");
		pr_err
		    ("  echo [queue work] [activate work] [execute work] [wq debugger] > wq_enable_logs\n");
		pr_err("  ex. echo 1 1 1 1  > wq_enable_logs, to enable all logs\n");
		pr_err
		    ("  ex. echo 1 0 0 1  > wq_enable_logs, to enable \"queue work\" & \"wq debug\" logs\n");
		pr_err("  Note, please try to enable [wq debugger] as possible as you can.\n");
	}
}

MT_DEBUG_ENTRY(wq_log);
static int mt_wq_log_show(struct seq_file *m, void *v)
{
	if (wq_tracing & WQ_DUMP_QUEUE_WORK)
		SEQ_printf(m, "wq: queue work log enabled\n");
	if (wq_tracing & WQ_DUMP_ACTIVE_WORK)
		SEQ_printf(m, "wq: active work log enabled\n");
	if (wq_tracing & WQ_DUMP_EXECUTE_WORK)
		SEQ_printf(m, "wq: execute work log enabled\n");
	if (wq_tracing == 0)
		SEQ_printf(m, "wq: no log enabled\n");

	if (wq_debugger_enable)
		SEQ_printf(m, "wq: wq debugger is enabled\n");
	else
		SEQ_printf(m, "wq: wq debugger is disabled\n");

	print_help(m);
	return 0;
}

static ssize_t mt_wq_log_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	int log_queue_work = 0, log_activate_work = 0, log_execute_work = 0;
	int log_wq_debugger_enable = 0;
	char buf[64];

	if (cnt > sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = '\0';
	pr_err("rex %s\n", buf);

	if (sscanf
	    (buf, "%d %d %d %d", &log_queue_work, &log_activate_work, &log_execute_work,
	     &log_wq_debugger_enable) == 4) {
		wq_tracing = 0;
		wq_tracing |= (log_queue_work ? WQ_DUMP_QUEUE_WORK : 0);
		wq_tracing |= (log_activate_work ? WQ_DUMP_ACTIVE_WORK : 0);
		wq_tracing |= (log_execute_work ? WQ_DUMP_EXECUTE_WORK : 0);

		wq_debugger_enable = log_wq_debugger_enable;
	} else
		print_help(NULL);

	return cnt;
}

#endif				/* CONFIG_MTK_WQ_DEBUG */

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

    if (cnt >= sizeof(buf)){
		printk("mt_pid input stream size to large.\n");
		return -EINVAL;
	}
	

    if (copy_from_user(&buf, ubuf, cnt))
	return -EFAULT;
	
	buf[cnt] = 0;  
	printk("mt_pid input stream:%s\n", buf);
	
    ret = strict_strtoul(buf, 10, &val);

	reboot_pid = val;
	if(reboot_pid > PID_MAX_DEFAULT)
	{
		printk("get reboot pid error %d.\n", reboot_pid);
		reboot_pid = 0;
		return -EFAULT;
	}

    printk("get reboot pid: %d.\n", reboot_pid);
  

    return cnt;

}
/*-------------------------------------------------------------------*/
static int __init init_mtsched_prof(void)
{
	struct proc_dir_entry *pe;
	if (!proc_mkdir("mtprof", NULL)) {
		return -1;
	}
	pe = proc_create("mtprof/sched", 0444, NULL, &mt_sched_debug_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtprof/cputime", 0664, NULL, &mt_cputime_fops);
    if (!pe)
	return -ENOMEM;
	pe = proc_create("mtprof/reboot_pid", 0660, NULL, &mt_pid_fops);
	if (!pe)
		return -ENOMEM;

#ifdef CONFIG_MTK_WQ_DEBUG
	pe = proc_create("mtprof/wq_dump", 0664, NULL, &mt_wq_dump_fops);
	if (!pe)
		return -ENOMEM;

	pe = proc_create("mtprof/wq_enable_logs", 0664, NULL, &mt_wq_log_fops);
	if (!pe)
		return -ENOMEM;
#endif

	mt_cpu_num = num_present_cpus();
	mt_cpu_info_head = kmalloc(mt_cpu_num * sizeof(struct mt_cpu_info), GFP_ATOMIC);
	if (mt_cpu_info_head == NULL) {
		return -ENOMEM;
	}
#ifdef CONFIG_MT_ENG_BUILD

	pe = proc_create("mtprof/status", 0666, NULL, &mt_status_fops);
	if (!pe)
		return -ENOMEM;
#endif
	return 0;
}

__initcall(init_mtsched_prof);
