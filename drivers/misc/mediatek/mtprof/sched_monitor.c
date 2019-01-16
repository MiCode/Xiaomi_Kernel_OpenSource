#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "prof_ctl.h"
#include <linux/module.h>
#include <linux/pid.h>

#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/interrupt.h>

#include <linux/mt_sched_mon.h>
#include <linux/stacktrace.h>

#include <linux/aee.h>
#include <linux/stacktrace.h>


#define WARN_ISR_DUR     3000000
#define WARN_SOFTIRQ_DUR 10000000
#define WARN_TASKLET_DUR 10000000
#define WARN_HRTIMER_DUR 3000000
#define WARN_STIMER_DUR  10000000

enum mt_event_type {
	evt_ISR = 1,
	evt_SOFTIRQ = 2,
	evt_TASKLET,
	evt_HRTIMER,
	evt_STIMER,
};

DEFINE_PER_CPU(struct sched_block_event, ISR_mon);
DEFINE_PER_CPU(struct sched_block_event, SoftIRQ_mon);
DEFINE_PER_CPU(struct sched_block_event, tasklet_mon);
DEFINE_PER_CPU(struct sched_block_event, hrt_mon);
DEFINE_PER_CPU(struct sched_block_event, sft_mon);
DEFINE_PER_CPU(struct sched_stop_event, IRQ_disable_mon);
DEFINE_PER_CPU(struct sched_stop_event, Preempt_disable_mon);
DEFINE_PER_CPU(int, mt_timer_irq);

/* TIMER debug */
DEFINE_PER_CPU(int, mtsched_mon_enabled);
DEFINE_PER_CPU(unsigned long long, local_timer_ts);
DEFINE_PER_CPU(unsigned long long, local_timer_te);

#ifdef CONFIG_MT_SCHED_MONITOR
/* Save stack trace */
static DEFINE_PER_CPU(struct stack_trace, MT_stack_trace);
static DEFINE_PER_CPU(unsigned long long, TS_irq_off);
#endif
/* [IRQ-disable] White List
 * Flags for special scenario*/
DEFINE_PER_CPU(int, MT_trace_in_sched);
DEFINE_PER_CPU(int, MT_trace_in_resume_console);

#define MAX_STACK_TRACE_DEPTH   32

static DEFINE_MUTEX(mt_sched_mon_lock);

void mt_sched_monitor_switch(int on);

/* //////////////////////////////////////////////////////// */
/* Some utility macro */
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

/*  */
/* //////////////////////////////////////////////////////// */
/* --------------------------------------------------- */
/* Real work */
#ifdef CONFIG_MT_SCHED_MONITOR
static const char *isr_name(int irq)
{
	struct irqaction *action;
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	if (!desc)
		return NULL;
	else {
		action = desc->action;
		if (!action)
			return NULL;
		else
			return action->name;
	}
}

static void event_duration_check(struct sched_block_event *b)
{
	unsigned long long t_dur;
	t_dur = b->last_te - b->last_ts;
	switch (b->type) {
	case evt_ISR:
		if (t_dur > WARN_ISR_DUR) {
			pr_err
			    ("[ISR DURATION WARN] IRQ[%d:%s], dur:%llu ns > %d ms,(s:%llu,e:%llu)\n",
			     (int)b->last_event, isr_name(b->last_event), t_dur,
			     WARN_ISR_DUR / 1000000, b->last_ts, b->last_te);
		}
        if(b->preempt_count != preempt_count()){
            pr_debug("[ISR WARN]IRQ[%d:%s], Unbalanced Preempt Count:0x%x! Should be 0x%x\n",(int)b->last_event, isr_name(b->last_event), preempt_count(), b->preempt_count);
        }

		break;
	case evt_SOFTIRQ:
		if (t_dur > WARN_SOFTIRQ_DUR) {
			struct sched_block_event *b_isr;
			b_isr = &__raw_get_cpu_var(ISR_mon);
			pr_err
			    ("[SOFTIRQ DURATION WARN] SoftIRQ:%d, dur:%llu ns > %d ms,(s:%llu,e:%llu)\n",
			     (int)b->last_event, t_dur, WARN_SOFTIRQ_DUR / 1000000, b->last_ts,
			     b->last_te);
			if (b_isr->last_ts > b->last_ts)	/* ISR occur during Tasklet */
			{
				pr_err
				    (" IRQ occurrs in this duration, IRQ[%d:%s], dur:%llu ns (s:%llu, e:%llu)\n\n",
				     (int)b_isr->last_event, isr_name(b_isr->last_event),
				     b_isr->last_te - b_isr->last_ts, b_isr->last_ts,
				     b_isr->last_te);
			}
		}
        if(b->preempt_count != preempt_count()){
            pr_debug("[SOFTIRQ WARN] SoftIRQ:%d, Unbalanced Preempt Count:0x%x! Should be 0x%x\n",(int)b->last_event, preempt_count(), b->preempt_count);
        }
		break;
	case evt_TASKLET:
		if (t_dur > WARN_TASKLET_DUR) {
			struct sched_block_event *b_isr;
			b_isr = &__raw_get_cpu_var(ISR_mon);
			pr_err
			    ("[TASKLET DURATION WARN] Tasklet:%pS, dur:%llu ns > %d ms,(s:%llu,e:%llu)\n",
			     (void *)b->last_event, t_dur, WARN_TASKLET_DUR / 1000000, b->last_ts,
			     b->last_te);
			if (b_isr->last_ts > b->last_ts)	/* ISR occur during Tasklet */
			{
				pr_err
				    (" IRQ occurrs in this duration, IRQ[%d:%s], dur:%llu ns (s:%llu, e:%llu)\n\n",
				     (int)b_isr->last_event, isr_name(b_isr->last_event),
				     b_isr->last_te - b_isr->last_ts, b_isr->last_ts,
				     b_isr->last_te);
			}
		}
        if(b->preempt_count != preempt_count()){
            pr_debug("[TASKLET WARN] TASKLET:%pS, Unbalanced Preempt Count:0x%x! Should be 0x%x\n",(void *)b->last_event, preempt_count(), b->preempt_count);
        }
		break;
	case evt_HRTIMER:
		if (t_dur > WARN_HRTIMER_DUR) {
			pr_err
			    ("[HRTIMER DURATION WARN] HRTIMER:%pS, dur:%llu ns > %d ms,(s:%llu,e:%llu)\n",
			     (void *)b->last_event, t_dur, WARN_HRTIMER_DUR / 1000000, b->last_ts,
			     b->last_te);
		}
        if(b->preempt_count != preempt_count()){
            pr_debug("[HRTIMER WARN] HRTIMER:%pS, Unbalanced Preempt Count:0x%x! Should be 0x%x\n",(void *)b->last_event, preempt_count(), b->preempt_count);
        }
		break;
	case evt_STIMER:
		if (t_dur > WARN_STIMER_DUR) {
			struct sched_block_event *b_isr;
			b_isr = &__raw_get_cpu_var(ISR_mon);
			pr_err
			    ("[STIMER DURATION WARN] SoftTIMER:%pS, dur:%llu ns > %d ms,(s:%llu,e:%llu)\n",
			     (void *)b->last_event, t_dur, WARN_STIMER_DUR / 1000000, b->last_ts,
			     b->last_te);
			if (b_isr->last_ts > b->last_ts)	/* ISR occur during Softtimer */
			{
				pr_err
				    (" IRQ occurrs in this duration, IRQ[%d:%s], dur:%llu ns (s:%llu, e:%llu)\n\n",
				     (int)b_isr->last_event, isr_name(b_isr->last_event),
				     b_isr->last_te - b_isr->last_ts, b_isr->last_ts,
				     b_isr->last_te);
			}
		}
        if(b->preempt_count != preempt_count()){
            pr_debug("[STTIMER WARN] SoftTIMER:%pS, Unbalanced Preempt Count:0x%x! Should be 0x%x\n",(void *)b->last_event, preempt_count(), b->preempt_count);
        }
		break;

	}
}

static void reset_event_count(struct sched_block_event *b)
{
	b->last_count = b->cur_count;
	b->cur_count = 0;
}

/* ISR monitor */
void mt_trace_ISR_start(int irq)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(ISR_mon);

    b->preempt_count = preempt_count();
	b->cur_ts = sched_clock();
	b->cur_event = (unsigned long)irq;
	aee_rr_rec_last_irq_enter(smp_processor_id(), irq, b->cur_ts);

}

void mt_trace_ISR_end(int irq)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(ISR_mon);

	WARN_ON(b->cur_event != irq);
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	b->cur_event = 0;
	b->cur_ts = 0;
	event_duration_check(b);
	aee_rr_rec_last_irq_exit(smp_processor_id(), irq, b->last_te);

	/* reset HRTimer function counter */
	b = &__raw_get_cpu_var(hrt_mon);
	reset_event_count(b);

}

/* SoftIRQ monitor */
void mt_trace_SoftIRQ_start(int sq_num)
{

	struct sched_block_event *b;
	b = &__raw_get_cpu_var(SoftIRQ_mon);

    b->preempt_count = preempt_count();
	b->cur_ts = sched_clock();
	b->cur_event = (unsigned long)sq_num;
}

void mt_trace_SoftIRQ_end(int sq_num)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(SoftIRQ_mon);

	WARN_ON(b->cur_event != sq_num);
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	b->cur_event = 0;
	b->cur_ts = 0;
	event_duration_check(b);

	/* reset soft timer function counter */
	b = &__raw_get_cpu_var(sft_mon);
	reset_event_count(b);
	/* reset tasklet function counter */
	b = &__raw_get_cpu_var(tasklet_mon);
	reset_event_count(b);
}

/* Tasklet monitor */
void mt_trace_tasklet_start(void *func)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(tasklet_mon);

    b->preempt_count = preempt_count();
	b->cur_ts = sched_clock();
	b->cur_event = (unsigned long)func;
	b->cur_count++;
}

void mt_trace_tasklet_end(void *func)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(tasklet_mon);

	WARN_ON(b->cur_event != (unsigned long)func);
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	b->cur_event = 0;
	b->cur_ts = 0;
	event_duration_check(b);
}

/* HRTimer monitor */
void mt_trace_hrt_start(void *func)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(hrt_mon);

    b->preempt_count = preempt_count();
	b->cur_ts = sched_clock();
	b->cur_event = (unsigned long)func;
	b->cur_count++;
}

void mt_trace_hrt_end(void *func)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(hrt_mon);

	WARN_ON(b->cur_event != (unsigned long)func);
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	b->cur_event = 0;
	b->cur_ts = 0;
	event_duration_check(b);
}

/* SoftTimer monitor */
void mt_trace_sft_start(void *func)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(sft_mon);

    b->preempt_count = preempt_count();
	b->cur_ts = sched_clock();
	b->cur_event = (unsigned long)func;
	b->cur_count++;
}

void mt_trace_sft_end(void *func)
{
	struct sched_block_event *b;
	b = &__raw_get_cpu_var(sft_mon);

	WARN_ON(b->cur_event != (unsigned long)func);
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	b->cur_event = 0;
	b->cur_ts = 0;
	event_duration_check(b);
}

/* Preempt off monitor */
void MT_trace_preempt_off(void)
{
	struct sched_stop_event *e;
	e = &__raw_get_cpu_var(Preempt_disable_mon);

	e->cur_ts = sched_clock();
}

void MT_trace_preempt_on(void)
{
	struct sched_stop_event *e;
	e = &__raw_get_cpu_var(Preempt_disable_mon);
	e->last_ts = e->cur_ts;
	e->cur_ts = 0;
	e->last_te = sched_clock();

}

/* IRQ off monitor */
void MT_trace_irq_off(void)
{
	struct sched_stop_event *e;
	struct stack_trace *trace;
	e = &__raw_get_cpu_var(IRQ_disable_mon);

	e->cur_ts = sched_clock();
	/*save timestap */
	__raw_get_cpu_var(TS_irq_off) = sched_clock();
	trace = &__raw_get_cpu_var(MT_stack_trace);
	/*save backtraces */
	trace->nr_entries = 0;
	trace->max_entries = MAX_STACK_TRACE_DEPTH;	/* 32 */
	trace->skip = 0;
	save_stack_trace_tsk(current, trace);


}

void MT_trace_irq_on(void)
{
	struct sched_stop_event *e;
	e = &__raw_get_cpu_var(IRQ_disable_mon);
	e->last_ts = e->cur_ts;
	e->cur_ts = 0;
	e->last_te = sched_clock();

}

#include <linux/irqnr.h>
#include <linux/kernel_stat.h>
#include <asm/hardirq.h>

int mt_irq_count[NR_CPUS][MAX_NR_IRQS];
#ifdef CONFIG_SMP
int mt_local_irq_count[NR_CPUS][NR_IPI];
#endif
unsigned long long mt_save_irq_count_time;

DEFINE_SPINLOCK(mt_irq_count_lock);
void mt_save_irq_counts(void)
{
	int irq, cpu;
	unsigned long flags;

	/* do not refresh data in 20ms */
	if (sched_clock() - mt_save_irq_count_time < 20000000)
		return;

	spin_lock_irqsave(&mt_irq_count_lock, flags);
	if (smp_processor_id() != 0) {	/* only record by CPU#0 */
		spin_unlock_irqrestore(&mt_irq_count_lock, flags);
		return;
	}
	mt_save_irq_count_time = sched_clock();
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			mt_irq_count[cpu][irq] = kstat_irqs_cpu(irq, cpu);
		}
	}
#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		for (irq = 0; irq < NR_IPI; irq++) {
			mt_local_irq_count[cpu][irq] = __get_irq_stat(cpu, ipi_irqs[irq]);
		}
	}
#endif
	spin_unlock_irqrestore(&mt_irq_count_lock, flags);
}
#else
void mt_trace_ISR_start(int id)
{
}

void mt_trace_ISR_end(int id)
{
}

void mt_trace_SoftIRQ_start(int id)
{
}

void mt_trace_SoftIRQ_end(int id)
{
}

void mt_trace_tasklet_start(void *func)
{
}

void mt_trace_tasklet_end(void *func)
{
}

void mt_trace_hrt_start(void *func)
{
}

void mt_trace_hrt_end(void *func)
{
}

void mt_trace_sft_start(void *func)
{
}

void mt_trace_sft_end(void *func)
{
}

void MT_trace_irq_on(void)
{
}

void MT_trace_irq_off(void)
{
}

void MT_trace_preempt_on(void)
{
}

void MT_trace_preempt_off(void)
{
}

void mt_save_irq_counts(void)
{
}
#endif
 /**/
#define TIME_3MS  3000000
#define TIME_10MS 10000000
#define TIME_20MS 20000000
#define TIME_1S   1000000000
static DEFINE_PER_CPU(int, MT_tracing_cpu);
static DEFINE_PER_CPU(unsigned long long, t_irq_on);
static DEFINE_PER_CPU(unsigned long long, t_irq_off);
void MT_trace_softirqs_on(unsigned long ip);
void MT_trace_softirqs_off(unsigned long ip);

void mt_dump_irq_off_traces(void);
void MT_trace_hardirqs_on(void)
{
	unsigned long long t_diff, t_on, t_off;
	if (unlikely(__raw_get_cpu_var(mtsched_mon_enabled) & 0x2)) {
		if (0 == current->pid)	/* Ignore swap thread */
			return;
		if (__raw_get_cpu_var(MT_trace_in_sched))
			return;
		if (__raw_get_cpu_var(MT_trace_in_resume_console))
			return;
		if (__raw_get_cpu_var(MT_tracing_cpu) == 1) {
			MT_trace_irq_on();
			t_on = sched_clock();
			t_off = __raw_get_cpu_var(t_irq_off);
			t_diff = t_on - t_off;

			__raw_get_cpu_var(t_irq_on) = t_on;
			if (t_diff > TIME_20MS) {
				pr_err
				    ("\n----------------------------[IRQ disable monitor]-------------------------\n");
				pr_err
				    ("[Sched Latency Warning:IRQ Disable too long(>20ms)] Duration: %lld.%lu ms (off:%lld.%lums, on:%lld.%lums)\n",
				     SPLIT_NS(t_diff), SPLIT_NS(t_off), SPLIT_NS(t_on));
				mt_dump_irq_off_traces();
				pr_err("irq on at: %lld.%lu ms\n", SPLIT_NS(t_on));
				pr_err("irq on backtraces:\n");
				dump_stack();
				pr_err
				    ("--------------------------------------------------------------------------\n\n");
			}
			__raw_get_cpu_var(t_irq_off) = 0;
		}
		__raw_get_cpu_var(MT_tracing_cpu) = 0;
	}
}
EXPORT_SYMBOL(MT_trace_hardirqs_on);
void MT_trace_hardirqs_off(void)
{
	if (unlikely(__raw_get_cpu_var(mtsched_mon_enabled) & 0x2)) {
		if (0 == current->pid)	/* Ignore swap thread */
			return;
		if (__raw_get_cpu_var(MT_trace_in_sched))
			return;
		if (__raw_get_cpu_var(MT_trace_in_resume_console))
			return;
		if (__raw_get_cpu_var(MT_tracing_cpu) == 0) {
			MT_trace_irq_off();
			__raw_get_cpu_var(t_irq_off) = sched_clock();
		}
		__raw_get_cpu_var(MT_tracing_cpu) = 1;
	}
}
EXPORT_SYMBOL(MT_trace_hardirqs_off);

void mt_dump_irq_off_traces(void)
{
#ifdef CONFIG_MT_SCHED_MONITOR
	int i;
	struct stack_trace *trace;
	trace = &__raw_get_cpu_var(MT_stack_trace);
	pr_err("irq off at:%lld.%lu ms\n", SPLIT_NS(__raw_get_cpu_var(TS_irq_off)));
	pr_err("irq off backtraces:\n");
	for (i = 0; i < trace->nr_entries; i++) {
		pr_err("[<%pK>] %pS\n", (void *)trace->entries[i], (void *)trace->entries[i]);
	}
#endif
}
EXPORT_SYMBOL(mt_dump_irq_off_traces);
/* --------------------------------------------------- */
/*                     Define Proc entry               */
/* --------------------------------------------------- */
MT_DEBUG_ENTRY(sched_monitor);
static int mt_sched_monitor_show(struct seq_file *m, void *v)
{
	int cpu;
	SEQ_printf(m, "=== mt Scheduler monitoring ===\n");
	SEQ_printf(m,
		   " 0: Disable All\n 1: [Preemption] Monitor\n 2: [IRQ disable] Monitor\n 3: Enable All\n");

	for_each_possible_cpu(cpu) {
		SEQ_printf(m, "  Scheduler Monitor:%d (CPU#%d)\n",
			   per_cpu(mtsched_mon_enabled, cpu), cpu);
	}

	return 0;
}

static ssize_t mt_sched_monitor_write(struct file *filp, const char *ubuf,
				      size_t cnt, loff_t *data)
{
	char buf[64];
	int val;
	int ret;
	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, (unsigned long *)&val);
	if (ret < 0)
		return ret;
	/* 0: off, 1:on */
	/* val = !!val; */
	if (val == 8)
		mt_dump_sched_traces();
	if (val == 18)		/* 0x12 */
		mt_dump_irq_off_traces();
	mt_sched_monitor_switch(val);
	pr_err(" to %d\n", val);
	return cnt;
}

void reset_sched_monitor(void)
{
}

void start_sched_monitor(void)
{
}

void stop_sched_monitor(void)
{
}

void mt_sched_monitor_switch(int on)
{
	int cpu;
	preempt_disable_notrace();
	mutex_lock(&mt_sched_mon_lock);
	for_each_possible_cpu(cpu) {
		pr_err("[mtprof] sched monitor on CPU#%d switch from %d to %d\n", cpu,
		       per_cpu(mtsched_mon_enabled, cpu), on);
		per_cpu(mtsched_mon_enabled, cpu) = on;	/* 0x1 || 0x2, IRQ & Preempt */
	}
	mutex_unlock(&mt_sched_mon_lock);
	preempt_enable_notrace();
}

static int __init init_mtsched_mon(void)
{
#ifdef CONFIG_MT_SCHED_MONITOR
	int cpu;
	struct proc_dir_entry *pe;
	for_each_possible_cpu(cpu) {
		per_cpu(MT_stack_trace, cpu).entries =
		    kmalloc(MAX_STACK_TRACE_DEPTH * 4, GFP_KERNEL);
		per_cpu(MT_tracing_cpu, cpu) = 0;
		per_cpu(mtsched_mon_enabled, cpu) = 0;	/* 0x1 || 0x2, IRQ & Preempt */

		per_cpu(ISR_mon, cpu).type = evt_ISR;
		per_cpu(SoftIRQ_mon, cpu).type = evt_SOFTIRQ;
		per_cpu(tasklet_mon, cpu).type = evt_TASKLET;
		per_cpu(hrt_mon, cpu).type = evt_HRTIMER;
		per_cpu(sft_mon, cpu).type = evt_STIMER;
	}

	if (!proc_mkdir("mtmon", NULL)) {
		return -1;
	}
	pe = proc_create("mtmon/sched_mon", 0664, NULL, &mt_sched_monitor_fops);
	if (!pe)
		return -ENOMEM;
#endif
	return 0;
}

__initcall(init_mtsched_mon);
