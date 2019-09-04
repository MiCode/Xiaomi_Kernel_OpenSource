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

#ifndef _MTK_SCHED_MON_H
#define _MTK_SCHED_MON_H
/*CPU holding event: ISR/SoftIRQ/Tasklet/Timer*/
struct sched_block_event {
	int type;
	unsigned long cur_event;
	unsigned long last_event;
	unsigned long long cur_ts;
	unsigned long long last_ts;
	unsigned long long last_te;

	unsigned long cur_count;
	unsigned long last_count;
	int preempt_count;
};

#define MAX_NR_IRQS 1500
struct mt_irq_count {
	unsigned int irqs[MAX_NR_IRQS];
};
#include <asm/hardirq.h>
#ifdef CONFIG_SMP
struct mt_local_irq_count {
	unsigned int ipis[NR_IPI];
};
#endif

/*Schedule disable event: IRQ/Preempt disable monitor*/
struct sched_stop_event {
	unsigned long long cur_ts;
	unsigned long long last_ts;
	unsigned long long last_te;
	int preempt_count;
};

struct sched_lock_event {
	unsigned long long lock_ts;
	unsigned long long lock_te;
	unsigned long long lock_dur;
	unsigned long lock_owner;
};

struct lock_block_event {
	unsigned long long try_lock_s;
	unsigned long long try_lock_e;
	unsigned long long last_spinning_s;
	unsigned long long last_spinning_e;
};

#define CPU_DOWN 1
#define SCHED_TICK 0

DECLARE_PER_CPU(struct sched_block_event, ISR_mon);
DECLARE_PER_CPU(struct sched_block_event, IPI_mon);
DECLARE_PER_CPU(struct sched_block_event, SoftIRQ_mon);
DECLARE_PER_CPU(struct sched_block_event, RCU_SoftIRQ_mon);
DECLARE_PER_CPU(struct sched_block_event, tasklet_mon);
DECLARE_PER_CPU(struct sched_block_event, hrt_mon);
DECLARE_PER_CPU(struct sched_block_event, sft_mon);
DECLARE_PER_CPU(struct sched_block_event, irq_work_mon);
DECLARE_PER_CPU(struct sched_stop_event, IRQ_disable_mon);
DECLARE_PER_CPU(struct sched_stop_event, Preempt_disable_mon);
DECLARE_PER_CPU(struct sched_lock_event, rq_lock_mon);
DECLARE_PER_CPU(struct lock_block_event, spinlock_mon);
DECLARE_PER_CPU(int, mt_timer_irq);
DECLARE_PER_CPU(int, mtsched_mon_enabled);
DECLARE_PER_CPU(struct mt_irq_count, irq_count_mon);
DECLARE_PER_CPU(struct mt_local_irq_count, ipi_count_mon);
DECLARE_PER_CPU(unsigned long long, save_irq_count_time);
/* [IRQ-disable] White List */
/* Flags for special scenario */
DECLARE_PER_CPU(int, MT_trace_in_sched);
DECLARE_PER_CPU(unsigned long long, local_timer_ts);
DECLARE_PER_CPU(unsigned long long, local_timer_te);

extern void mt_trace_ISR_start(int id);
extern void mt_trace_ISR_end(int id);
extern void mt_trace_IPI_start(int id);
extern void mt_trace_IPI_end(int id);
extern void mt_trace_SoftIRQ_start(int id);
extern void mt_trace_SoftIRQ_end(int id);
extern void mt_trace_tasklet_start(void *func);
extern void mt_trace_tasklet_end(void *func);
extern void mt_trace_hrt_start(void *func);
extern void mt_trace_hrt_end(void *func);
extern void mt_trace_sft_start(void *func);
extern void mt_trace_sft_end(void *func);
extern void mt_save_irq_counts(int action);
extern void mt_trace_rqlock_start(raw_spinlock_t *lock);
extern void mt_trace_rqlock_end(raw_spinlock_t *lock);
extern void mt_trace_irq_work_start(void *func);
extern void mt_trace_irq_work_end(void *func);
extern void mt_trace_RCU_SoftIRQ_start(void *func);
extern void mt_trace_RCU_SoftIRQ_end(void);
extern void mt_trace_lock_spinning_start(raw_spinlock_t *lock);
extern void mt_trace_lock_spinning_end(raw_spinlock_t *lock);

#ifdef CONFIG_PREEMPT_MONITOR
extern void MT_trace_irq_on(void);
extern void MT_trace_irq_off(void);
extern void MT_trace_preempt_on(void);
extern void MT_trace_preempt_off(void);
extern void MT_trace_check_preempt_dur(void);
extern void MT_trace_hardirqs_on(void);
extern void MT_trace_hardirqs_off(void);
#else
static inline void MT_trace_irq_on(void)
{
};

static inline void MT_trace_irq_off(void)
{
};

static inline void MT_trace_preempt_on(void)
{
};

static inline void MT_trace_preempt_off(void)
{
};

static inline void MT_trace_check_preempt_dur(void)
{
};

static inline void MT_trace_hardirqs_on(void)
{
};

static inline void MT_trace_hardirqs_off(void)
{
};
#endif

extern spinlock_t mt_irq_count_lock;
extern void mt_show_last_irq_counts(void);
extern void mt_show_current_irq_counts(void);
extern void mt_sched_monitor_switch(int on);
extern void mt_aee_dump_sched_traces(void);
extern void mt_dump_sched_traces(void);

#endif				/* _MTK_SCHED_MON_H */
