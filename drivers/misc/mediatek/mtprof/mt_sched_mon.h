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

#ifndef _MT_SCHED_MON_H
#define _MT_SCHED_MON_H
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

#define MAX_NR_IRQS 512
struct mt_irq_count {
	unsigned int irqs[MAX_NR_IRQS];
};
#include <asm/hardirq.h>
#ifdef CONFIG_SMP
struct mt_local_irq_count {
	unsigned int ipis[NR_IPI];
};
#endif

#define CPU_DOWN 1
#define SCHED_TICK 0

DECLARE_PER_CPU(struct sched_block_event, ISR_mon);
DECLARE_PER_CPU(struct sched_block_event, SoftIRQ_mon);
DECLARE_PER_CPU(struct sched_block_event, tasklet_mon);
DECLARE_PER_CPU(struct sched_block_event, hrt_mon);
DECLARE_PER_CPU(struct sched_block_event, sft_mon);
DECLARE_PER_CPU(struct mt_irq_count, irq_count_mon);
DECLARE_PER_CPU(struct mt_local_irq_count, ipi_count_mon);
DECLARE_PER_CPU(unsigned long long, save_irq_count_time);

DECLARE_PER_CPU(int, mt_timer_irq);
#ifdef CONFIG_MT_SCHED_MONITOR
extern void mt_trace_ISR_start(int id);
extern void mt_trace_ISR_end(int id);
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
#else
static inline void mt_trace_ISR_start(int id) {};
static inline void mt_trace_ISR_end(int id) {};
static inline void mt_trace_SoftIRQ_start(int id) {};
static inline void mt_trace_SoftIRQ_end(int id) {};
static inline void mt_trace_tasklet_start(void *func) {};
static inline void mt_trace_tasklet_end(void *func) {};
static inline void mt_trace_hrt_start(void *func) {};
static inline void mt_trace_hrt_end(void *func) {};
static inline void mt_trace_sft_start(void *func) {};
static inline void mt_trace_sft_end(void *func) {};
static inline void mt_save_irq_counts(int action) {};
static inline void mt_trace_rqlock_start(raw_spinlock_t *lock) {};
static inline void mt_trace_rqlock_end(raw_spinlock_t *lock) {};
#endif

extern spinlock_t mt_irq_count_lock;
extern void mt_show_last_irq_counts(void);
extern void mt_show_current_irq_counts(void);
extern void mt_sched_monitor_switch(int on);


/*Schedule disable event: IRQ/Preempt disable monitor*/
struct sched_stop_event {
	unsigned long long cur_ts;
	unsigned long long last_ts;
	unsigned long long last_te;
	unsigned long long lock_dur;
	unsigned long lock_owner;
	raw_spinlock_t *lock;
};

struct sched_lock_event {
	unsigned long long lock_ts;
	unsigned long long lock_te;
	unsigned long long lock_dur;
	unsigned long lock_owner;
};

DECLARE_PER_CPU(struct sched_stop_event, IRQ_disable_mon);
DECLARE_PER_CPU(struct sched_stop_event, Preempt_disable_mon);
DECLARE_PER_CPU(struct sched_lock_event, Raw_spin_lock_mon);
DECLARE_PER_CPU(struct sched_lock_event, rq_lock_mon);

#ifdef CONFIG_PREEMPT_MONITOR
extern void MT_trace_irq_on(void);
extern void MT_trace_irq_off(void);
extern void MT_trace_preempt_on(void);
extern void MT_trace_preempt_off(void);
extern void MT_trace_check_preempt_dur(void);
extern void MT_trace_raw_spin_lock_s(raw_spinlock_t *lock);
extern void MT_trace_raw_spin_lock_e(raw_spinlock_t *lock);
#else
static inline void MT_trace_irq_on(void) {};
static inline void MT_trace_irq_off(void) {};
static inline void MT_trace_preempt_on(void) {};
static inline void MT_trace_preempt_off(void) {};
static inline void MT_trace_check_preempt_dur(void) {};
static inline void MT_trace_raw_spin_lock_s(raw_spinlock_t *lock) {};
static inline void MT_trace_raw_spin_lock_e(raw_spinlock_t *lock) {};
#endif

/* [IRQ-disable] White List
 * Flags for special scenario*/
DECLARE_PER_CPU(int, MT_trace_in_sched);
DECLARE_PER_CPU(int, MT_trace_in_resume_console);

extern void mt_aee_dump_sched_traces(void);
extern void mt_dump_sched_traces(void);

DECLARE_PER_CPU(int, mtsched_mon_enabled);
DECLARE_PER_CPU(unsigned long long, local_timer_ts);
DECLARE_PER_CPU(unsigned long long, local_timer_te);

#include <linux/sched.h>

#define MON_STOP 0
#define MON_START 1
#define MON_RESET 2

#ifdef CONFIG_MT_RT_THROTTLE_MON
DECLARE_PER_CPU(struct mt_rt_mon_struct, mt_rt_mon_head);
DECLARE_PER_CPU(int, rt_mon_count);
DECLARE_PER_CPU(int, mt_rt_mon_enabled);
DECLARE_PER_CPU(unsigned long long, rt_start_ts);
DECLARE_PER_CPU(unsigned long long, rt_end_ts);
DECLARE_PER_CPU(unsigned long long, rt_dur_ts);

extern void save_mt_rt_mon_info(int cpu, u64 delta_exec, struct task_struct *p);
extern void mt_rt_mon_switch(int on, int cpu);
extern void mt_rt_mon_print_task(int cpu);
extern void mt_rt_mon_print_task_from_buffer(void);
extern void update_mt_rt_mon_start(int cpu, u64 delta_exec);
extern int mt_rt_mon_enable(int cpu);
#else
static inline void
save_mt_rt_mon_info(int cpu, u64 delta_exec, struct task_struct *p) {};
static inline void mt_rt_mon_switch(int on, int cpu) {};
static inline void mt_rt_mon_print_task(int cpu) {};
static inline void mt_rt_mon_print_task_from_buffer(void) {};
static inline void update_mt_rt_mon_start(int cpu, u64 delta_exec) {};
static inline int mt_rt_mon_enable(int cpu)
{
	return 0;
}
#endif
#endif /* _MT_SCHED_MON_H */

