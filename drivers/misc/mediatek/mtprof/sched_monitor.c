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

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/interrupt.h>
#include <linux/stacktrace.h>
#include <linux/stacktrace.h>
#include <mt-plat/aee.h>
#include "mtk_sched_mon.h"
#include "internal.h"
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>
#endif

enum mt_event_type {
	evt_ISR = 1,
	evt_SOFTIRQ = 2,
	evt_TASKLET,
	evt_HRTIMER,
	evt_STIMER,
	evt_IPI,
};

static const char * const softirq_name[] = {
	"HI_SOFTIRQ",
	"TIMER_SOFTIRQ",
	"NET_TX_SOFTIRQ",
	"NET_RX_SOFTIRQ",
	"BLOCK_SOFTIRQ",
	"BLOCK_IOPOLL_SOFTIRQ",
	"TASKLET_SOFTIRQ",
	"SCHED_SOFTIRQ",
	"HRTIMER_SOFTIRQ",
	"RCU_SOFTIRQ"
};

#define MAX_STACK_TRACE_DEPTH   32

#define TIME_1MS	1000000
#define TIME_3MS	3000000
#define TIME_10MS	10000000
#define TIME_200MS	200000000
#define TIME_500MS	500000000

static unsigned int WARN_ISR_DUR = TIME_10MS;
static unsigned int WARN_SOFTIRQ_DUR = TIME_10MS;
static unsigned int WARN_TASKLET_DUR = TIME_10MS;
static unsigned int WARN_HRTIMER_DUR = TIME_10MS;
static unsigned int WARN_STIMER_DUR = TIME_10MS;
static unsigned int WARN_BURST_IRQ_DETECT = 25000;
static unsigned int WARN_PREEMPT_DUR = TIME_3MS;
static unsigned int WARN_IRQ_DISABLE_DUR = TIME_3MS;
static unsigned int AEE_WARN_DUR = TIME_500MS;

static unsigned int irq_info_enable;
static unsigned int sched_mon_enable;
static unsigned int sched_mon_warn_enable;
static unsigned int sched_mon_door_key;
static unsigned int skip_aee_once;

/* //////////////////////////////////////////////////////// */
DEFINE_PER_CPU(struct sched_block_event, ISR_mon);
DEFINE_PER_CPU(struct sched_block_event, IPI_mon);
DEFINE_PER_CPU(struct sched_block_event, SoftIRQ_mon);
DEFINE_PER_CPU(struct sched_block_event, tasklet_mon);
DEFINE_PER_CPU(struct sched_block_event, hrt_mon);
DEFINE_PER_CPU(struct sched_block_event, sft_mon);
DEFINE_PER_CPU(struct sched_stop_event, IRQ_disable_mon);
DEFINE_PER_CPU(struct sched_stop_event, Preempt_disable_mon);
DEFINE_PER_CPU(struct sched_lock_event, Raw_spin_lock_mon);
DEFINE_PER_CPU(struct sched_lock_event, rq_lock_mon);
DEFINE_PER_CPU(int, mt_timer_irq);
DEFINE_PER_CPU(int, mtsched_mon_enabled);
/* [IRQ-disable] White List */
/* Flags for special scenario */
DEFINE_PER_CPU(int, MT_trace_in_sched);
DEFINE_PER_CPU(unsigned long long, local_timer_ts);
DEFINE_PER_CPU(unsigned long long, local_timer_te);
static DEFINE_PER_CPU(int, MT_tracing_cpu);
#ifdef CONFIG_PREEMPT_MONITOR
static DEFINE_PER_CPU(unsigned long long, t_irq_on);
static DEFINE_PER_CPU(unsigned long long, t_irq_off);
static DEFINE_PER_CPU(unsigned long long, TS_irq_off);
#endif

/* Save stack trace */
static DEFINE_PER_CPU(struct stack_trace, MT_stack_trace);


static DEFINE_MUTEX(mt_sched_mon_lock);

#define SPLIT_NS_H(x) nsec_high(x)
#define SPLIT_NS_L(x) nsec_low(x)
/* //////////////////////////////////////////////////////// */

/* --------------------------------------------------- */
static const char *task_name(void *task)
{
	struct task_struct *p = NULL;

	p = task;
	if (p)
		return p->comm;
	return NULL;
}

static void sched_monitor_aee(struct sched_block_event *b)
{
	char aee_str[60];
	unsigned long long t_dur;

	t_dur = b->last_te - b->last_ts;
	switch (b->type) {
	case evt_ISR:
		snprintf(aee_str, 60, "SCHED MONITOR : ISR DURATION WARN");
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
			aee_str, "ISR DURATION WARN: IRQ[%d:%s] dur:%llu ns",
			(int)b->last_event, isr_name(b->last_event), t_dur);
		break;
	case evt_SOFTIRQ:
		snprintf(aee_str, 60, "SCHED MONITOR : SOFTIRQ DURATION WARN");
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
			aee_str,
			"SOFTIRQ DURATION WARN: SoftIRQ:%d dur:%llu ns",
			(int)b->last_event, t_dur);
		break;
	}
}

/* Real work */
static void event_duration_check(struct sched_block_event *b)
{
	unsigned long long t_dur;

	t_dur = b->last_te - b->last_ts;

	if (!sched_mon_enable)
		return;
	switch (b->type) {
	case evt_ISR:
		if (t_dur > WARN_ISR_DUR) {
			if (sched_mon_warn_enable || t_dur > AEE_WARN_DUR) {
				pr_info("[ISR DURATION WARN%s] IRQ[%d:%s] dur:%llu ms > %d ms (s:%llu,e:%llu)\n",
					t_dur > AEE_WARN_DUR ? "!" : "",
					(int)b->last_event,
					isr_name(b->last_event),
					nsec_high(t_dur),
					WARN_ISR_DUR / 1000000,
					b->last_ts,
					b->last_te);
			}
			if (t_dur > AEE_WARN_DUR) {
				if (!skip_aee_once)
					sched_monitor_aee(b);
				skip_aee_once = 0;
			}
		}
		if (b->preempt_count != preempt_count()) {
			pr_info("[ISR WARN]IRQ[%d:%s] Unbalanced Preempt Count:0x%x! Should be 0x%x\n",
				(int)b->last_event,
				isr_name(b->last_event),
				preempt_count(),
				b->preempt_count);
		}
		break;
	case evt_SOFTIRQ:
		if (t_dur > WARN_SOFTIRQ_DUR) {
			struct sched_block_event *b_isr;

			b_isr = &__raw_get_cpu_var(ISR_mon);
			if (sched_mon_warn_enable || t_dur > AEE_WARN_DUR) {
				pr_info("[SOFTIRQ DURATION WARN%s] SoftIRQ:%d[%s] dur:%llu ms > %d ms (s:%llu,e:%llu)\n",
					t_dur > AEE_WARN_DUR ? "!" : "",
					(int)b->last_event,
					softirq_name[(int)b->last_event],
					nsec_high(t_dur),
					WARN_SOFTIRQ_DUR / 1000000,
					b->last_ts,
					b->last_te);

				/* ISR occur during SOFTIRQ */
				if (irq_info_enable == 1
					&& b_isr->last_ts > b->last_ts) {
					pr_info(" IRQ occurrs in this duration, IRQ[%d:%s], dur:%llu ns (s:%llu,e:%llu)\n",
						(int)b_isr->last_event,
						isr_name(b_isr->last_event),
						b_isr->last_te - b_isr->last_ts,
						b_isr->last_ts,
						b_isr->last_te);
				}
			}
			/* Should skip (b->last_event != RCU_SOFTIRQ)? */
			if (t_dur > AEE_WARN_DUR)
				sched_monitor_aee(b);
		}
		if (b->preempt_count != preempt_count()) {
			pr_info("[SOFTIRQ WARN] SoftIRQ:%d, Unbalanced Preempt Count:0x%x! Should be 0x%x\n",
				(int)b->last_event,
				preempt_count(),
				b->preempt_count);
		}
		break;
	case evt_TASKLET:
		if ((sched_mon_warn_enable && t_dur > WARN_TASKLET_DUR)
			|| (t_dur > AEE_WARN_DUR)) {
			struct sched_block_event *b_isr;

			b_isr = &__raw_get_cpu_var(ISR_mon);
			pr_info("[TASKLET DURATION WARN] Tasklet:%pS dur:%llu ms > %d ms (s:%llu,e:%llu)\n",
				(void *)b->last_event,
				nsec_high(t_dur),
				WARN_TASKLET_DUR / 1000000,
				b->last_ts,
				b->last_te);

			/* ISR occur during Tasklet */
			if (irq_info_enable == 1
				&& b_isr->last_ts > b->last_ts) {
				pr_info(" IRQ occurrs in this dur,IRQ[%d:%s], dur:%llu ns (s:%llu,e:%llu)\n",
					(int)b_isr->last_event,
					isr_name(b_isr->last_event),
					b_isr->last_te - b_isr->last_ts,
					b_isr->last_ts,
					b_isr->last_te);
			}
		}
		if (b->preempt_count != preempt_count()) {
			pr_info("[TASKLET WARN] TASKLET:%pS, Unbalanced Preempt Count:0x%x! Should be 0x%x\n",
				(void *)b->last_event,
				preempt_count(),
				b->preempt_count);
		}
		break;
	case evt_HRTIMER:
		if ((sched_mon_warn_enable && t_dur > WARN_HRTIMER_DUR)
			|| (t_dur > AEE_WARN_DUR)) {
			struct sched_lock_event *lock_e;
			static char htimer_name[128];

			/* tick_sched_timer
			 *  -> irq_work_tick -> call_console_drivers
			 * Too much log to console triggers duration warning on
			 * tick_sched_timer. This is a false alarm.
			 */
			snprintf(htimer_name, sizeof(htimer_name), "%ps",
			(void *)b->last_event);
			if (strcmp(htimer_name, "tick_sched_timer") == 0)
				skip_aee_once = 1;

			lock_e = &__raw_get_cpu_var(rq_lock_mon);
			pr_info("[HRTIMER DURATION WARN] HRTIMER:%pS dur:%llu ms > %d ms (s:%llu,e:%llu)\n",
				(void *)b->last_event,
				nsec_high(t_dur),
				WARN_HRTIMER_DUR / 1000000,
				b->last_ts,
				b->last_te);

			if (irq_info_enable == 1
				&& lock_e->lock_owner
				&& lock_e->lock_ts > b->last_ts
				&& lock_e->lock_dur > TIME_1MS) {
				pr_info("[HRTIMER WARN] get rq->lock,last owner:%s dur: %llu ns(s:%llu,e:%llu)\n",
					task_name((void *)lock_e->lock_owner),
					lock_e->lock_dur,
					usec_high(lock_e->lock_ts),
					usec_high(lock_e->lock_te));
			}
		}
		if (b->preempt_count != preempt_count()) {
			pr_info("[HRTIMER WARN] HRTIMER:%pS, Unbalanced Preempt Count:0x%x! Should be 0x%x\n",
				(void *)b->last_event,
				preempt_count(),
				b->preempt_count);
		}
		break;
	case evt_STIMER:
		if ((sched_mon_warn_enable && t_dur > WARN_STIMER_DUR)
			|| (t_dur > AEE_WARN_DUR)) {
			struct sched_block_event *b_isr;

			b_isr = &__raw_get_cpu_var(ISR_mon);
			pr_info("[STIMER DURATION WARN] SoftTIMER:%pS dur:%llu ms > %d ms (s:%llu,e:%llu)\n",
				(void *)b->last_event,
				nsec_high(t_dur),
				WARN_STIMER_DUR / 1000000,
				b->last_ts,
				b->last_te);

			/* ISR occur during Softtimer */
			if (irq_info_enable == 1
				&& b_isr->last_ts > b->last_ts) {
				pr_info(" IRQ occurrs in this duration, IRQ[%d:%s], dur:%llu ns (s:%llu,e:%llu)\n",
					(int)b_isr->last_event,
					isr_name(b_isr->last_event),
					b_isr->last_te - b_isr->last_ts,
					b_isr->last_ts,
					b_isr->last_te);
			}
		}
		if (b->preempt_count != preempt_count()) {
			pr_info("[STTIMER WARN] SoftTIMER:%pS, Unbalanced Preempt Count:0x%x! Should be 0x%x\n",
				(void *)b->last_event,
				preempt_count(),
				b->preempt_count);
		}
		break;
	case evt_IPI:
		if ((sched_mon_warn_enable && t_dur > WARN_ISR_DUR)
			|| (t_dur > AEE_WARN_DUR)) {
			pr_info("[ISR DURATION WARN] IPI[%d] dur:%llu ms > %d ms (s:%llu,e:%llu)\n",
				(int)b->last_event,
				nsec_high(t_dur),
				WARN_ISR_DUR / 1000000,
				b->last_ts,
				b->last_te);
		}
		if (b->preempt_count != preempt_count()) {
			pr_info("[IPI WARN]IRQ[%d:%s], Unbalanced Preempt Count:0x%x! Should be 0x%x\n",
				(int)b->last_event, isr_name(b->last_event),
				preempt_count(),
				b->preempt_count);
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
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_last_irq_enter(smp_processor_id(), irq, b->cur_ts);
#endif
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
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_last_irq_exit(smp_processor_id(), irq, b->last_te);
#endif

	/* reset HRTimer function counter */
	b = &__raw_get_cpu_var(hrt_mon);
	reset_event_count(b);

}
/* ISR monitor */
void mt_trace_IPI_start(int ipinr)
{
	struct sched_block_event *b;

	b = &__raw_get_cpu_var(IPI_mon);

	b->preempt_count = preempt_count();
	b->cur_ts = sched_clock();
	b->cur_event = (unsigned long)ipinr;
}

void mt_trace_IPI_end(int ipinr)
{
	struct sched_block_event *b;

	b = &__raw_get_cpu_var(IPI_mon);

	WARN_ON(b->cur_event != ipinr);
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	b->cur_event = 0;
	b->cur_ts = 0;
	event_duration_check(b);

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
	struct sched_lock_event *lock_e;

	b = &__raw_get_cpu_var(hrt_mon);

	WARN_ON(b->cur_event != (unsigned long)func);
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	b->cur_event = 0;
	b->cur_ts = 0;
	event_duration_check(b);

	lock_e = &__raw_get_cpu_var(rq_lock_mon);
	lock_e->lock_dur = 0;

}

/*trace hrtimer : schedule_tick rq lock time check*/
void mt_trace_rqlock_start(raw_spinlock_t *lock)
{
	struct sched_lock_event *lock_e;
	struct task_struct *owner = NULL;

#ifdef CONFIG_DEBUG_SPINLOCK
	if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
		owner = lock->owner;
#endif
	lock_e = &__raw_get_cpu_var(rq_lock_mon);

	lock_e->lock_ts = sched_clock();
	lock_e->lock_owner = (unsigned long)owner;
}

void mt_trace_rqlock_end(raw_spinlock_t *lock)
{
	struct sched_lock_event *lock_e;
#ifdef CONFIG_DEBUG_SPINLOCK
	struct task_struct *owner = NULL;


	if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
		owner = lock->owner;
#endif
	lock_e = &__raw_get_cpu_var(rq_lock_mon);

	lock_e->lock_te = sched_clock();
	lock_e->lock_dur = lock_e->lock_te - lock_e->lock_ts;
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

/*IRQ Counts monitor & IRQ Burst monitor*/
#include <linux/irqnr.h>
#include <linux/kernel_stat.h>
#include <asm/hardirq.h>

DEFINE_PER_CPU(struct mt_irq_count, irq_count_mon);
DEFINE_PER_CPU(struct mt_local_irq_count, ipi_count_mon);
DEFINE_PER_CPU(unsigned long long, save_irq_count_time);

DEFINE_SPINLOCK(mt_irq_count_lock);
static void burst_irq_check(int irq, int irq_num, unsigned long long t_diff,
	unsigned long long t_start, unsigned long long t_end)
{
	int count, old_irq;
	unsigned long long t_avg;
	struct mt_irq_count *irq_count;

	irq_count = &__raw_get_cpu_var(irq_count_mon);

	old_irq = irq_count->irqs[irq];
	count = irq_num - old_irq;
	if (count != 0) {
		t_avg = t_diff;
		do_div(t_avg, count);
		if (t_avg < WARN_BURST_IRQ_DETECT) {
			pr_info("[BURST IRQ DURATION WARN] IRQ[%3d:%14s] +%d (dur:%lld us, avg:%lld us, %lld ~ %lld us)\n",
				irq,
				isr_name(irq),
				count,
				usec_high(t_diff),
				usec_high(t_avg),
				usec_high(t_start),
				usec_high(t_end));
		}
	}
}

void mt_save_irq_counts(int action)
{
	int irq, cpu, irq_num;
	unsigned long flags;
	unsigned long long t_start, t_end;

	/* do not refresh data in 200ms */
	if (action == SCHED_TICK &&
		 (sched_clock() - __raw_get_cpu_var(save_irq_count_time)
		 < TIME_200MS))
		return;

	spin_lock_irqsave(&mt_irq_count_lock, flags);

	cpu = smp_processor_id();

	t_end = sched_clock();
	t_start = __raw_get_cpu_var(save_irq_count_time);
	__raw_get_cpu_var(save_irq_count_time) = sched_clock();

	for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
		irq_num = kstat_irqs_cpu(irq, cpu);
		burst_irq_check(irq, irq_num, t_end - t_start, t_start, t_end);
		__raw_get_cpu_var(irq_count_mon).irqs[irq] = irq_num;
	}

#ifdef CONFIG_SMP
	for (irq = 0; irq < NR_IPI; irq++)
		__raw_get_cpu_var(ipi_count_mon).ipis[irq] = __get_irq_stat(
							cpu, ipi_irqs[irq]);
#endif
	spin_unlock_irqrestore(&mt_irq_count_lock, flags);
}

#ifdef CONFIG_PREEMPT_MONITOR
/* Preempt off monitor */
void MT_trace_preempt_off(void)
{
	struct sched_stop_event *e;
	int cpu = smp_processor_id();
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *curr = rq->curr;

	if (unlikely(__raw_get_cpu_var(mtsched_mon_enabled) & 0x1)) {
		if (strncmp(curr->comm, "swapper", 7)
		    && strncmp(curr->comm, "migration", 9)
		    && !in_interrupt()) {
			e = &__raw_get_cpu_var(Preempt_disable_mon);
			e->cur_ts = sched_clock();
			e->preempt_count = preempt_count();
		}
	}
}

void MT_trace_preempt_on(void)
{
	struct sched_stop_event *e;
	unsigned long long t_dur = 0;
	int cpu = smp_processor_id();
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *curr = rq->curr;

	if (unlikely(__raw_get_cpu_var(mtsched_mon_enabled) & 0x1)) {
		if (strncmp(curr->comm, "swapper", 7)
		    && strncmp(curr->comm, "migration", 9)
		    && !in_interrupt()) {
			e = &__raw_get_cpu_var(Preempt_disable_mon);
			if (preempt_count() == e->preempt_count) {
				e->last_ts = e->cur_ts;
				e->last_te = sched_clock();
				t_dur = e->last_te - e->last_ts;
				if (t_dur != e->last_te)
					curr->preempt_dur = t_dur;
			}
		}
	}
}

void MT_trace_check_preempt_dur(void)
{
	struct sched_stop_event *e;
	struct sched_block_event *b_isr;
	unsigned long long t_dur = 0;
	unsigned long long t_dur_isr = 0;

	if (unlikely(__raw_get_cpu_var(mtsched_mon_enabled) & 0x1)) {
		b_isr = &__raw_get_cpu_var(ISR_mon);
		e = &__raw_get_cpu_var(Preempt_disable_mon);
		t_dur = current->preempt_dur;

		if (t_dur > WARN_PREEMPT_DUR && e->last_ts > 0) {
			pr_info("[PREEMPT DURATION WARN] dur:%llu ms (s:%llu,e:%llu)\n",
				nsec_high(t_dur),
				usec_high(e->last_ts),
				usec_high(e->last_te));

			if (b_isr->last_ts > e->last_ts) {
				t_dur_isr = b_isr->last_te - b_isr->cur_ts;
				pr_info("IRQ occurrs in this duration, IRQ[%d:%s] dur %llu (s:%llu,e:%llu)\n",
					(int)b_isr->last_event,
					isr_name(b_isr->last_event),
					t_dur_isr,
					usec_high(b_isr->last_ts),
					usec_high(b_isr->last_te));
			}
#ifdef CONFIG_MTK_SCHED_MON_DEFAULT_ENABLE
			if (oops_in_progress == 0)
				aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
					"SCHED MONITOR : PREEMPT DURATION WARN",
					"PREEMPT DURATION WARN dur:%llu ns",
					t_dur);
#endif
		}
		current->preempt_dur = 0;
		e->cur_ts = 0;
		e->last_te = 0;
		e->last_ts = 0;
	}
}

#ifdef CONFIG_DEBUG_SPINLOCK
void MT_trace_spin_lock_start(raw_spinlock_t *lock)
{
	struct sched_lock_event *lock_e;
	struct task_struct *owner = NULL;

	if (unlikely(__raw_get_cpu_var(mtsched_mon_enabled) & 0x1)) {
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
			owner = lock->owner;

		lock_e = &__raw_get_cpu_var(Raw_spin_lock_mon);
		lock_e->lock_ts = sched_clock();
		lock_e->lock_owner = (unsigned long)owner;
	}
}

void MT_trace_spin_lock_end(raw_spinlock_t *lock)
{
	struct sched_lock_event *lock_e;
	struct task_struct *owner = NULL;

	if (unlikely(__raw_get_cpu_var(mtsched_mon_enabled) & 0x1)) {
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
			owner = lock->owner;
		lock_e = &__raw_get_cpu_var(Raw_spin_lock_mon);
		lock_e->lock_te = sched_clock();
		lock_e->lock_dur = lock_e->lock_te - lock_e->lock_ts;
		lock_e->lock_owner = (unsigned long)owner;
		if (lock_e->lock_dur > WARN_PREEMPT_DUR) {
			pr_info("[SPINLOCK DURATION WARN] spin time:%llu ns (s:%llu,e:%llu) spinlock owenr:%s lock:%pS\n",
				lock_e->lock_dur,
				usec_high(lock_e->lock_ts),
				usec_high(lock_e->lock_te),
				task_name((void *)lock_e->lock_owner), lock);
		}
	}
}
#endif				/*CONFIG_DEBUG_SPINLOCK */

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

void mt_dump_irq_off_traces(void)
{
	int i;
	struct stack_trace *trace;

	trace = &__raw_get_cpu_var(MT_stack_trace);
	pr_debug("irq off at:%lld.%lu ms\n",
		 SPLIT_NS_H(__raw_get_cpu_var(TS_irq_off)),
		 SPLIT_NS_L(__raw_get_cpu_var(TS_irq_off)));
	pr_debug("irq off backtraces:\n");
	for (i = 0; i < trace->nr_entries; i++)
		pr_debug("[<%pK>] %pS\n",
		(void *)trace->entries[i],
		(void *)trace->entries[i]);
}
EXPORT_SYMBOL(mt_dump_irq_off_traces);

void MT_trace_hardirqs_on(void)
{
	unsigned long long t_diff, t_on, t_off;

	if (unlikely(__raw_get_cpu_var(mtsched_mon_enabled) & 0x2)) {
#ifdef CONFIG_TRACE_IRQFLAGS
		if (current->hardirqs_enabled)
			return;
#endif
		if (current->pid == 0)	/* Ignore swap thread */
			return;
		if (__raw_get_cpu_var(MT_trace_in_sched))
			return;
		if (__raw_get_cpu_var(MT_tracing_cpu) == 1) {
			MT_trace_irq_on();
			t_on = sched_clock();
			t_off = __raw_get_cpu_var(t_irq_off);
			t_diff = t_on - t_off;

			__raw_get_cpu_var(t_irq_on) = t_on;
			if (t_diff > WARN_IRQ_DISABLE_DUR) {
				pr_debug("\n-----[IRQ disable monitor]----\n");
				pr_debug("[Sched Latency Warning:IRQ Disable");
				pr_debug(" too long(>%lldms)],",
					nsec_high(WARN_IRQ_DISABLE_DUR));
				pr_debug("Duration: %lld.%lu ms",
					SPLIT_NS_H(t_diff),
					SPLIT_NS_L(t_diff));
				pr_debug(" (off:%lld.%lums, on:%lld.%lums)\n",
					SPLIT_NS_H(t_off),
					SPLIT_NS_L(t_off),
					SPLIT_NS_H(t_on),
					SPLIT_NS_L(t_on));


#ifdef CONFIG_TRACE_IRQFLAGS
				pr_debug("hardirqs last  enabled at (%u): ",
					current->hardirq_enable_event);
				print_ip_sym(current->hardirq_enable_ip);
				pr_debug("hardirqs last disabled at (%u): ",
					current->hardirq_disable_event);
				print_ip_sym(current->hardirq_disable_ip);
#endif
				mt_dump_irq_off_traces();
				pr_debug("irq on at: %lld.%lu ms\n",
					SPLIT_NS_H(t_on),
					SPLIT_NS_L(t_on));
				pr_debug("irq on backtraces:\n");
				dump_stack();
				pr_debug("----------------------------------");
				pr_debug("------------------------------\n\n");
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
#ifdef CONFIG_TRACE_IRQFLAGS
		if (!current->hardirqs_enabled)
			return;
#endif
		if (current->pid == 0)	/* Ignore swap thread */
			return;
		if (__raw_get_cpu_var(MT_trace_in_sched))
			return;
		if (__raw_get_cpu_var(MT_tracing_cpu) == 0) {
			MT_trace_irq_off();
			__raw_get_cpu_var(t_irq_off) = sched_clock();
		}
		__raw_get_cpu_var(MT_tracing_cpu) = 1;
	}
}
EXPORT_SYMBOL(MT_trace_hardirqs_off);

#endif				/*CONFIG_PREEMPT_MONITOR */

/* --------------------------------------------------- */
/*                     Define Proc entry               */
/* --------------------------------------------------- */
MT_DEBUG_ENTRY(sched_monitor);
static int mt_sched_monitor_show(struct seq_file *m, void *v)
{
	int cpu;

	SEQ_printf(m, "=== mt Scheduler monitoring ===\n");
	SEQ_printf(m, " 0: Disable All\n");
	SEQ_printf(m, " 1: [Preemption] Monitor\n");
	SEQ_printf(m, " 2: [IRQ disable] Monitor\n");
	SEQ_printf(m, " 3: Enable All\n");

	for_each_possible_cpu(cpu) {
		SEQ_printf(m, "  Scheduler Monitor:%d (CPU#%d)\n",
			   per_cpu(mtsched_mon_enabled, cpu), cpu);
	}

	return 0;
}

void mt_sched_monitor_switch(int on)
{
	int cpu;

	preempt_disable_notrace();
	mutex_lock(&mt_sched_mon_lock);
	for_each_possible_cpu(cpu) {
		pr_debug("[mtprof] sched monitor on CPU#%d switch from %d to %d\n",
			cpu, per_cpu(mtsched_mon_enabled, cpu), on);
		/* 0x1 || 0x2, IRQ & Preempt */
		per_cpu(mtsched_mon_enabled, cpu) = on;
	}
	mutex_unlock(&mt_sched_mon_lock);
	preempt_enable_notrace();
}

static ssize_t mt_sched_monitor_write(struct file *filp, const char *ubuf,
				      size_t cnt, loff_t *data)
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
	/* 0: off, 1:on */
	/* val = !!val; */
	if (val == 8)
		mt_dump_sched_traces();
#ifdef CONFIG_PREEMPT_MONITOR
	if (val == 18)		/* 0x12 */
		mt_dump_irq_off_traces();
#endif
	mt_sched_monitor_switch(val);
	pr_info(" to %lu\n", val);
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

#define DECLARE_MT_SCHED_MATCH(param, warn_dur)			\
static ssize_t mt_sched_monitor_##param##_write(			\
	struct file *filp,					\
	const char *ubuf,				\
	size_t cnt, loff_t *data)				\
{									\
	char buf[64];							\
	unsigned long val;						\
	int ret;							\
									\
	if (!sched_mon_door_key)			\
		return cnt;						\
									\
	if (cnt >= sizeof(buf))					\
		return -EINVAL;						\
									\
	if (copy_from_user(&buf, ubuf, cnt))	\
		return -EFAULT;						\
									\
	buf[cnt] = 0;							\
	ret = kstrtoul(buf, 10, &val);			\
	if (ret < 0)							\
		return ret;						\
									\
	warn_dur = val;							\
	pr_info(" to %lu\n", val);               \
									\
	return cnt;							\
									\
}								\
								\
static int mt_sched_monitor_##param##_show(			\
	struct seq_file *m,					\
	void *v)						\
{									\
		SEQ_printf(m,			\
			   "%d\n", warn_dur);	\
		return 0;				\
}								\
static int mt_sched_monitor_##param##_open(struct inode *inode, \
						struct file *file) \
{ \
	return single_open(file, mt_sched_monitor_##param##_show, \
						inode->i_private); \
} \
\
static const struct file_operations mt_sched_monitor_##param##_fops = { \
	.open = mt_sched_monitor_##param##_open, \
	.write = mt_sched_monitor_##param##_write,\
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

DECLARE_MT_SCHED_MATCH(ISR_DUR, WARN_ISR_DUR);
DECLARE_MT_SCHED_MATCH(SOFTIRQ_DUR, WARN_SOFTIRQ_DUR);
DECLARE_MT_SCHED_MATCH(TASKLET_DUR, WARN_TASKLET_DUR);
DECLARE_MT_SCHED_MATCH(HRTIMER_DUR, WARN_HRTIMER_DUR);
DECLARE_MT_SCHED_MATCH(STIMER_DUR, WARN_STIMER_DUR);
DECLARE_MT_SCHED_MATCH(PREEMPT_DUR, WARN_PREEMPT_DUR);
DECLARE_MT_SCHED_MATCH(BURST_IRQ, WARN_BURST_IRQ_DETECT);
DECLARE_MT_SCHED_MATCH(IRQ_DISABLE_DUR, WARN_IRQ_DISABLE_DUR);
DECLARE_MT_SCHED_MATCH(IRQ_INFO_ENABLE, irq_info_enable);
DECLARE_MT_SCHED_MATCH(AEE_WARNING_DUR, AEE_WARN_DUR);
DECLARE_MT_SCHED_MATCH(SCHED_MON_ENABLE, sched_mon_enable);
DECLARE_MT_SCHED_MATCH(SCHED_MON_WARN_ENABLE, sched_mon_warn_enable);

static ssize_t mt_sched_monitor_door_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[24];

	if (cnt >= sizeof(buf))
		return cnt;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt-1] = 0;

	if (strcmp("ENABLE_setting", buf) == 0)
		sched_mon_door_key = 1;
	if (strcmp("DISABLE_setting", buf) == 0)
		sched_mon_door_key = 0;

	return cnt;
}

static const struct file_operations mt_sched_monitor_door_fops = {
	.open = simple_open,
	.write = mt_sched_monitor_door_write,
};

static int __init init_mtsched_mon(void)
{
	int cpu;
	struct proc_dir_entry *pe;

	for_each_possible_cpu(cpu) {
		per_cpu(MT_stack_trace, cpu).entries =
		    kmalloc(MAX_STACK_TRACE_DEPTH * sizeof(unsigned long),
			GFP_KERNEL);
		per_cpu(MT_tracing_cpu, cpu) = 0;
		 /* 0x1 || 0x2, IRQ & Preempt */
		per_cpu(mtsched_mon_enabled, cpu) = 0;

		per_cpu(ISR_mon, cpu).type = evt_ISR;
		per_cpu(IPI_mon, cpu).type = evt_IPI;
		per_cpu(SoftIRQ_mon, cpu).type = evt_SOFTIRQ;
		per_cpu(tasklet_mon, cpu).type = evt_TASKLET;
		per_cpu(hrt_mon, cpu).type = evt_HRTIMER;
		per_cpu(sft_mon, cpu).type = evt_STIMER;
	}

	if (!proc_mkdir("mtmon", NULL))
		return -1;
	pe = proc_create("mtmon/sched_mon", 0664, NULL, &mt_sched_monitor_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_ISR", 0664, NULL,
			 &mt_sched_monitor_ISR_DUR_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_SOFTIRQ", 0664, NULL,
			 &mt_sched_monitor_SOFTIRQ_DUR_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_TASKLET", 0664, NULL,
			 &mt_sched_monitor_TASKLET_DUR_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_HRTIMER", 0664, NULL,
			 &mt_sched_monitor_HRTIMER_DUR_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_STIMER", 0664, NULL,
			 &mt_sched_monitor_STIMER_DUR_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_BURST_IRQ", 0664, NULL,
			 &mt_sched_monitor_BURST_IRQ_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_PREEMPT", 0664, NULL,
			 &mt_sched_monitor_PREEMPT_DUR_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_IRQ_DISABLE", 0664, NULL,
			 &mt_sched_monitor_IRQ_DISABLE_DUR_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_duration_AEE_WARNING", 0664, NULL,
			 &mt_sched_monitor_AEE_WARNING_DUR_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/irq_info_enable", 0664, NULL,
			 &mt_sched_monitor_IRQ_INFO_ENABLE_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_enable", 0664, NULL,
			 &mt_sched_monitor_SCHED_MON_ENABLE_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_warn_enable", 0664, NULL,
			 &mt_sched_monitor_SCHED_MON_WARN_ENABLE_fops);
	if (!pe)
		return -ENOMEM;
	pe = proc_create("mtmon/sched_mon_door", 0220, NULL,
			&mt_sched_monitor_door_fops);
	if (!pe)
		return -ENOMEM;

	sched_mon_enable = 1;
	sched_mon_warn_enable = 0;

	return 0;
}

device_initcall(init_mtsched_mon);
