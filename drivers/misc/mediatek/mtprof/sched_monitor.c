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
#include <mt-plat/aee.h>
#include "mtk_sched_mon.h"
#include "internal.h"
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>
#endif
#define CREATE_TRACE_POINTS
#include "mtk_sched_mon_trace.h"

enum sched_mon_event {
	evt_ISR,
	evt_SOFTIRQ,
	evt_TASKLET,
	evt_HRTIMER,
	evt_STIMER,
	evt_IPI,
	evt_IRQWORK,
	evt_HARDIRQ,
	evt_BURST_IRQ,
	evt_PREEMPT_CNT,
	NUM_EVENTS
};

struct sched_mon_event_count {
	const char *name;
	unsigned long warn_cnt;
	unsigned long aee_cnt;
};
static struct sched_mon_event_count sched_mon_evt[NUM_EVENTS] = {
	{"ISR", 0, 0},
	{"SOFTIRQ", 0, 0},
	{"TASKLET", 0, 0},
	{"HRTIMER", 0, 0},
	{"STIMER", 0, 0},
	{"IPI", 0, 0},
	{"IRQWORK", 0, 0},
	{"HARDIRQ", 0, 0},
	{"BURST_IRQ", 0, 0},
	{"PREEMPT_CNT", 0, 0}
};

#define en_ISR          0x001
#define en_SOFTIRQ      0x002
#define en_TASKLET      0x004
#define en_HRTIMER      0x008
#define en_STIMER       0x010
#define en_IPI          0x020
#define en_IRQWORK      0x040
#define en_HARDIRQ      0x080
#define en_BURST_IRQ    0x100
#define en_PREEMPT_CNT  0x200

#define TO_KERNEL_LOG  0x1
#define TO_FTRACE      0x2
#define TO_DEFERRED    0x4
#define TO_BOTH  (TO_KERNEL_LOG|TO_FTRACE)
#define TO_BOTH_SAVE  (TO_DEFERRED|TO_FTRACE)

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
#define TIME_5MS	5000000
#define TIME_10MS	10000000
#define TIME_50MS	50000000
#define TIME_100MS	100000000
#define TIME_200MS	200000000
#define TIME_500MS	500000000
#define BUFFER_TIME	10000000

static unsigned int WARN_ISR_DUR = TIME_3MS;
static unsigned int WARN_SOFTIRQ_DUR = TIME_5MS;
static unsigned int WARN_TASKLET_DUR = TIME_5MS;
static unsigned int WARN_HRTIMER_DUR = TIME_3MS;
static unsigned int WARN_STIMER_DUR = TIME_5MS;
static unsigned int WARN_BURST_IRQ_DETECT = 25000;
static unsigned int WARN_PREEMPT_DUR = TIME_3MS;
static unsigned int WARN_IRQ_DISABLE_DUR = TIME_50MS;
static unsigned int WARN_IRQ_WORK_DUR = TIME_500MS - BUFFER_TIME;
static unsigned int AEE_COMMON_DUR = TIME_500MS;
static unsigned int AEE_IRQ_DISABLE_DUR = TIME_500MS;
static unsigned int sched_mon_enable;
static unsigned int sched_mon_door_key;
static unsigned int irq_info_enable;
static unsigned int skip_aee;
static unsigned int warning_on;
static unsigned int sched_mon_func_select;
static char *aee_buf;
static char buf_tasklet[144];
static char buf_hrtimer[144];
static char buf_stimer[144];

/* enable to generate aee db */
static unsigned int aee_on_duration;
static unsigned int aee_on_preempt_balance;
static unsigned int aee_on_burst_irq;

/* limit quantity of aee db */
static int aee_cnt_duration;
static int aee_cnt_preempt_balance;
static int aee_cnt_burst_irq;

DEFINE_PER_CPU(struct sched_block_event, ISR_mon);
DEFINE_PER_CPU(struct sched_block_event, IPI_mon);
DEFINE_PER_CPU(struct sched_block_event, SoftIRQ_mon);
DEFINE_PER_CPU(struct sched_block_event, RCU_SoftIRQ_mon);
DEFINE_PER_CPU(struct sched_block_event, tasklet_mon);
DEFINE_PER_CPU(struct sched_block_event, hrt_mon);
DEFINE_PER_CPU(struct sched_block_event, sft_mon);
DEFINE_PER_CPU(struct sched_block_event, irq_work_mon);
DEFINE_PER_CPU(struct sched_stop_event, IRQ_disable_mon);
DEFINE_PER_CPU(struct sched_stop_event, Preempt_disable_mon);
DEFINE_PER_CPU(struct sched_lock_event, rq_lock_mon);
DEFINE_PER_CPU(struct lock_block_event, spinlock_mon);
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

/* --------------------------------------------------- */
static const char *task_name(void *task)
{
	struct task_struct *p = NULL;

	p = task;
	if (p)
		return p->comm;
	return NULL;
}

static void sched_mon_msg(char *buf, int out)
{
	if (out & TO_FTRACE)
		trace_sched_mon_msg(buf);
	if (out & TO_KERNEL_LOG)
		pr_info("%s\n", buf);
	if (out & TO_DEFERRED)
		printk_deferred("%s\n", buf);
}

static void sched_monitor_aee(unsigned int event, const char *msg)
{
	char aee_str[64];

	sched_mon_evt[event].aee_cnt++;

	if (!aee_on_duration || !(aee_cnt_duration-- > 0))
		return;

	switch (event) {
	case evt_ISR:
		snprintf(aee_str, 64, "SCHED MONITOR : ISR DURATION WARN");
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE, aee_str,
			"ISR DURATION WARN\n%s\n%s", msg, aee_buf);
		aee_buf = "";
		break;
	case evt_SOFTIRQ:
		snprintf(aee_str, 64, "SCHED MONITOR : SOFTIRQ DURATION WARN");
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE, aee_str,
			"SOFTIRQ DURATION WARN\n%s\n%s", msg, aee_buf);
		aee_buf = "";
		break;
	case evt_HARDIRQ:
		snprintf(aee_str, 64,
			"SCHED MONITOR : IRQ DISABLE DURATION WARN");
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE, aee_str,
			"IRQ DISABLE DURATION WARN\n%s", msg);
		break;
	case evt_BURST_IRQ:
		snprintf(aee_str, 64,
			"SCHED MONITOR : BURST IRQ DURATION WARN");
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_DEFAULT | DB_OPT_FTRACE, aee_str,
			"BURST IRQ DURATION WARN\n%s", msg);
		break;
	case evt_PREEMPT_CNT:
		if (aee_on_preempt_balance &&
			aee_cnt_preempt_balance-- > 0) {
			snprintf(aee_str, 64,
				"SCHED MONITOR : UNBALANCED PREEMPT COUNT WARN");
			aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DEFAULT | DB_OPT_FTRACE, aee_str,
				"UNBALANCED PREEMPT COUNT WARN\n%s", msg);
		}
		break;
	}
}

/* Real work */
static void event_duration_check(struct sched_block_event *b)
{
	unsigned long long t_dur;
	char buf[144];

	if (!sched_mon_enable)
		return;

	t_dur = b->last_te - b->last_ts;

	switch (b->type) {
	case evt_ISR:
		if (t_dur > WARN_ISR_DUR) {
			sched_mon_evt[b->type].warn_cnt++;
			if (warning_on || t_dur > AEE_COMMON_DUR) {
				snprintf(buf, sizeof(buf),
					"[ISR DURATION WARN%s] IRQ[%d:%s] dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)",
					t_dur > AEE_COMMON_DUR ? "!" : "",
					(int)b->last_event,
					isr_name(b->last_event),
					msec_high(t_dur),
					sec_high(b->last_ts),
					sec_low(b->last_ts),
					sec_high(b->last_te),
					sec_low(b->last_te));
				sched_mon_msg(buf, TO_BOTH);
			}
			if (t_dur > AEE_COMMON_DUR) {
				if (!skip_aee)
					sched_monitor_aee(b->type, buf);
				skip_aee = 0;
			}
		}
		if (b->preempt_count != preempt_count()) {
			snprintf(buf, sizeof(buf),
				"[ISR WARN]IRQ[%d:%s] Unbalanced Preempt Count:0x%x! Should be 0x%x",
				(int)b->last_event,
				isr_name(b->last_event),
				preempt_count(),
				b->preempt_count);
			sched_mon_msg(buf, TO_BOTH);
			sched_monitor_aee(evt_PREEMPT_CNT, buf);
		}
		break;
	case evt_SOFTIRQ:
		if (t_dur > WARN_SOFTIRQ_DUR) {
			struct sched_block_event *b_isr;

			sched_mon_evt[b->type].warn_cnt++;
			b_isr = &__raw_get_cpu_var(ISR_mon);
			if (warning_on || t_dur > AEE_COMMON_DUR) {
				snprintf(buf, sizeof(buf),
					"[SOFTIRQ DURATION WARN%s] SoftIRQ:%d[%s] dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)",
					t_dur > AEE_COMMON_DUR ? "!" : "",
					(int)b->last_event,
					softirq_name[(int)b->last_event],
					msec_high(t_dur),
					sec_high(b->last_ts),
					sec_low(b->last_ts),
					sec_high(b->last_te),
					sec_low(b->last_te));
				sched_mon_msg(buf, TO_BOTH);

				/* ISR occur during SOFTIRQ */
				if (irq_info_enable == 1
					&& b_isr->last_ts > b->last_ts) {
					char buf2[128];

					snprintf(buf2, sizeof(buf2),
						" IRQ occurrs in this duration, IRQ[%d:%s], dur:%llu ns (s:%lld.%06lu,e:%lld.%06lu)",
						(int)b_isr->last_event,
						isr_name(b_isr->last_event),
						b_isr->last_te - b_isr->last_ts,
						sec_high(b_isr->last_ts),
						sec_low(b_isr->last_ts),
						sec_high(b_isr->last_te),
						sec_low(b_isr->last_te));
					sched_mon_msg(buf2, TO_BOTH);
				}
			}
			if (t_dur > AEE_COMMON_DUR
				&& b->last_event != RCU_SOFTIRQ) {
				sched_monitor_aee(b->type, buf);
			}
		}
		if (b->preempt_count != preempt_count()) {
			snprintf(buf, sizeof(buf),
				"[SOFTIRQ WARN] SoftIRQ:%d, Unbalanced Preempt Count:0x%x! Should be 0x%x",
				(int)b->last_event,
				preempt_count(),
				b->preempt_count);
			sched_mon_msg(buf, TO_BOTH);
			sched_monitor_aee(evt_PREEMPT_CNT, buf);
		}
		break;
	case evt_TASKLET:
		if ((warning_on && t_dur > WARN_TASKLET_DUR)
			|| (t_dur > AEE_COMMON_DUR)) {
			struct sched_block_event *b_isr;

			sched_mon_evt[b->type].warn_cnt++;
			b_isr = &__raw_get_cpu_var(ISR_mon);
			snprintf(buf, sizeof(buf),
				"[TASKLET DURATION WARN] Tasklet:%ps dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)",
				(void *)b->last_event,
				msec_high(t_dur),
				sec_high(b->last_ts), sec_low(b->last_ts),
				sec_high(b->last_te), sec_low(b->last_te));
			sched_mon_msg(buf, TO_BOTH);
			if (t_dur > AEE_COMMON_DUR) {
				sched_mon_evt[b->type].aee_cnt++;
				strncpy(buf_tasklet, buf,
					sizeof(buf_tasklet) - 1);
				aee_buf = buf_tasklet;
			}

			/* ISR occur during Tasklet */
			if (irq_info_enable == 1
				&& b_isr->last_ts > b->last_ts) {
				snprintf(buf, sizeof(buf),
					" IRQ occurrs in this dur,IRQ[%d:%s], dur:%llu ns (s:%lld.%06lu,e:%lld.%06lu)",
					(int)b_isr->last_event,
					isr_name(b_isr->last_event),
					b_isr->last_te - b_isr->last_ts,
					sec_high(b_isr->last_ts),
					sec_low(b_isr->last_ts),
					sec_high(b_isr->last_te),
					sec_low(b_isr->last_te));
				sched_mon_msg(buf, TO_BOTH);
			}
		}
		if (b->preempt_count != preempt_count()) {
			snprintf(buf, sizeof(buf),
				"[TASKLET WARN] TASKLET:%ps, Unbalanced Preempt Count:0x%x! Should be 0x%x",
				(void *)b->last_event,
				preempt_count(),
				b->preempt_count);
			sched_mon_msg(buf, TO_BOTH);
			sched_monitor_aee(evt_PREEMPT_CNT, buf);
		}
		break;
	case evt_HRTIMER:
		if ((warning_on && t_dur > WARN_HRTIMER_DUR)
			|| (t_dur > AEE_COMMON_DUR)) {
			struct sched_lock_event *lock_e;
			static char htimer_name[64];

			sched_mon_evt[b->type].warn_cnt++;
			/* tick_sched_timer -> irq_work_tick
			 *                  -> wake_up_klogd_work_func
			 * Too much log to console triggers duration warning on
			 * tick_sched_timer. This is a false alarm.
			 */
			snprintf(htimer_name, sizeof(htimer_name), "%ps",
			(void *)b->last_event);
			if (strcmp(htimer_name, "tick_sched_timer") == 0)
				skip_aee = 1;

			lock_e = &__raw_get_cpu_var(rq_lock_mon);
			snprintf(buf, sizeof(buf),
				"[HRTIMER DURATION WARN] HRTIMER:%ps dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)",
				(void *)b->last_event,
				msec_high(t_dur),
				sec_high(b->last_ts), sec_low(b->last_ts),
				sec_high(b->last_te), sec_low(b->last_te));
			sched_mon_msg(buf, TO_BOTH);
			if (t_dur > AEE_COMMON_DUR) {
				sched_mon_evt[b->type].aee_cnt++;
				strncpy(buf_hrtimer, buf,
					sizeof(buf_hrtimer) - 1);
				aee_buf = buf_hrtimer;
			}

			if (irq_info_enable == 1
				&& lock_e->lock_owner
				&& lock_e->lock_ts > b->last_ts
				&& lock_e->lock_dur > TIME_1MS) {
				snprintf(buf, sizeof(buf),
					"[HRTIMER WARN] get rq->lock,last owner:%s dur: %llu ns (s:%lld.%06lu,e:%lld.%06lu)",
					task_name((void *)lock_e->lock_owner),
					lock_e->lock_dur,
					sec_high(lock_e->lock_ts),
					sec_low(lock_e->lock_ts),
					sec_high(lock_e->lock_te),
					sec_low(lock_e->lock_te));
				sched_mon_msg(buf, TO_BOTH);
			}
		}
		if (b->preempt_count != preempt_count()) {
			snprintf(buf, sizeof(buf),
				"[HRTIMER WARN] HRTIMER:%ps, Unbalanced Preempt Count:0x%x! Should be 0x%x",
				(void *)b->last_event,
				preempt_count(),
				b->preempt_count);
			sched_mon_msg(buf, TO_BOTH);
			sched_monitor_aee(evt_PREEMPT_CNT, buf);
		}
		break;
	case evt_STIMER:
		if ((warning_on && t_dur > WARN_STIMER_DUR)
			|| (t_dur > AEE_COMMON_DUR)) {
			struct sched_block_event *b_isr;

			sched_mon_evt[b->type].warn_cnt++;
			b_isr = &__raw_get_cpu_var(ISR_mon);
			snprintf(buf, sizeof(buf),
				"[STIMER DURATION WARN] SoftTIMER:%ps dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)",
				(void *)b->last_event,
				msec_high(t_dur),
				sec_high(b->last_ts), sec_low(b->last_ts),
				sec_high(b->last_te), sec_low(b->last_te));
			sched_mon_msg(buf, TO_BOTH);
			if (t_dur > AEE_COMMON_DUR) {
				sched_mon_evt[b->type].aee_cnt++;
				strncpy(buf_stimer, buf,
					sizeof(buf_stimer) - 1);
				aee_buf = buf_stimer;
			}

			/* ISR occur during Softtimer */
			if (irq_info_enable == 1
				&& b_isr->last_ts > b->last_ts) {
				snprintf(buf, sizeof(buf),
					" IRQ occurrs in this duration, IRQ[%d:%s], dur:%llu ns (s:%lld.%06lu,e:%lld.%06lu)",
					(int)b_isr->last_event,
					isr_name(b_isr->last_event),
					b_isr->last_te - b_isr->last_ts,
					sec_high(b_isr->last_ts),
					sec_low(b_isr->last_ts),
					sec_high(b_isr->last_te),
					sec_low(b_isr->last_te));
				sched_mon_msg(buf, TO_BOTH);
			}
		}
		if (b->preempt_count != preempt_count()) {
			snprintf(buf, sizeof(buf),
				"[STTIMER WARN] SoftTIMER:%ps, Unbalanced Preempt Count:0x%x! Should be 0x%x",
				(void *)b->last_event,
				preempt_count(),
				b->preempt_count);
			sched_mon_msg(buf, TO_BOTH);
			sched_monitor_aee(evt_PREEMPT_CNT, buf);
		}
		break;
	case evt_IPI:
		if ((warning_on && t_dur > WARN_ISR_DUR)
			|| (t_dur > AEE_COMMON_DUR)) {
			sched_mon_evt[b->type].warn_cnt++;
			snprintf(buf, sizeof(buf),
				"[ISR DURATION WARN] IPI[%d] dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)",
				(int)b->last_event,
				msec_high(t_dur),
				sec_high(b->last_ts), sec_low(b->last_ts),
				sec_high(b->last_te), sec_low(b->last_te));
			sched_mon_msg(buf, TO_BOTH);
			if (t_dur > AEE_COMMON_DUR)
				sched_mon_evt[b->type].aee_cnt++;
		}
		if (b->preempt_count != preempt_count()) {
			snprintf(buf, sizeof(buf),
				"[IPI WARN]IRQ[%d:%s], Unbalanced Preempt Count:0x%x! Should be 0x%x",
				(int)b->last_event, isr_name(b->last_event),
				preempt_count(),
				b->preempt_count);
			sched_mon_msg(buf, TO_BOTH);
			sched_monitor_aee(evt_PREEMPT_CNT, buf);
		}
		break;
	case evt_IRQWORK:
		if (t_dur > WARN_IRQ_WORK_DUR) {
			sched_mon_evt[b->type].warn_cnt++;
			snprintf(buf, sizeof(buf),
				"[IRQ WORK DURATION WARN] func: %ps, dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)",
				(void *)b->last_event,
				msec_high(t_dur),
				sec_high(b->last_ts), sec_low(b->last_ts),
				sec_high(b->last_te), sec_low(b->last_te));
			sched_mon_msg(buf, TO_BOTH);
			if (t_dur > AEE_COMMON_DUR)
				sched_mon_evt[b->type].aee_cnt++;
		}
		if (b->preempt_count != preempt_count()) {
			snprintf(buf, sizeof(buf),
				"[IRQ WORK WARN] func: %ps, Unbalanced Preempt Count:0x%x! Should be 0x%x",
				(void *)b->last_event,
				preempt_count(),
				b->preempt_count);
			sched_mon_msg(buf, TO_BOTH);
			sched_monitor_aee(evt_PREEMPT_CNT, buf);
		}
		break;
	}
}

static void softirq_event_duration_check(struct sched_block_event *b)
{
	unsigned long long t_dur;
	char buf[144];

	if (!sched_mon_enable)
		return;

	t_dur = b->last_te - b->last_ts;

	switch (b->type) {
	case RCU_SOFTIRQ:
		if (t_dur > WARN_SOFTIRQ_DUR) {
			if (warning_on
				|| t_dur > (AEE_COMMON_DUR - BUFFER_TIME)) {
				snprintf(buf, sizeof(buf),
					"[RCU_SOFTIRQ DURATION WARN] func: %ps, dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)",
					(void *)b->last_event,
					msec_high(t_dur),
					sec_high(b->last_ts),
					sec_low(b->last_ts),
					sec_high(b->last_te),
					sec_low(b->last_te));
				sched_mon_msg(buf, TO_BOTH);
				if (t_dur > (AEE_COMMON_DUR - BUFFER_TIME)) {
					aee_buf = "";
					sched_monitor_aee(evt_SOFTIRQ, buf);
				}
			}
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

/* IRQ work monitor */
void mt_trace_irq_work_start(void *func)
{
	struct sched_block_event *b;

	b = &__raw_get_cpu_var(irq_work_mon);

	b->preempt_count = preempt_count();
	b->cur_ts = sched_clock();
	b->cur_event = (unsigned long)func;
	b->cur_count++;
}

void mt_trace_irq_work_end(void *func)
{
	struct sched_block_event *b;

	b = &__raw_get_cpu_var(irq_work_mon);

	WARN_ON(b->cur_event != (unsigned long)func);
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	event_duration_check(b);
}

/* RCU_SOFTIRQ monitor */
void mt_trace_RCU_SoftIRQ_start(void *func)
{
	struct sched_block_event *b;

	b = &__raw_get_cpu_var(RCU_SoftIRQ_mon);

	b->cur_ts = sched_clock();
	b->cur_event = (unsigned long)func;
}

void mt_trace_RCU_SoftIRQ_end(void)
{
	struct sched_block_event *b;

	b = &__raw_get_cpu_var(RCU_SoftIRQ_mon);

	/* do not check event in RCU_SOFTIRQ case */
	b->last_event = b->cur_event;
	b->last_ts = b->cur_ts;
	b->last_te = sched_clock();
	b->cur_event = 0;
	b->cur_ts = 0;
	softirq_event_duration_check(b);
}

void mt_trace_lock_spinning_start(raw_spinlock_t *lock)
{
	struct lock_block_event *b;

	b = &__raw_get_cpu_var(spinlock_mon);
	b->try_lock_s = sched_clock();
}

void mt_trace_lock_spinning_end(raw_spinlock_t *lock)
{
	struct lock_block_event *b;

	b = &__raw_get_cpu_var(spinlock_mon);
	b->try_lock_e = sched_clock();
	if ((b->try_lock_e - b->try_lock_s > TIME_100MS)
		&& (b->try_lock_e > b->try_lock_s)) {
		b->last_spinning_s = b->try_lock_s;
		b->last_spinning_e = b->try_lock_e;
	}
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
			char buf[128];

			sched_mon_evt[evt_BURST_IRQ].warn_cnt++;
			snprintf(buf, sizeof(buf),
				"[BURST IRQ DURATION WARN] IRQ[%3d:%14s] +%d (dur:%lld us, avg:%lld us, %lld ~ %lld us)\n",
				irq,
				isr_name(irq),
				count,
				usec_high(t_diff),
				usec_high(t_avg),
				usec_high(t_start),
				usec_high(t_end));
			sched_mon_msg(buf, TO_BOTH);

			if (aee_on_burst_irq && aee_cnt_burst_irq-- > 0) {
				aee_buf = "";
				sched_monitor_aee(evt_BURST_IRQ, buf);
			}
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
			pr_info("[PREEMPT DURATION WARN] dur:%llu ms (s:%lld.%06lu,e:%lld.%06lu)\n",
				msec_high(t_dur),
				sec_high(e->last_ts), sec_low(e->last_ts),
				sec_high(e->last_te), sec_low(e->last_te));

			if (b_isr->last_ts > e->last_ts) {
				t_dur_isr = b_isr->last_te - b_isr->cur_ts;
				pr_info("IRQ occurrs in this duration, IRQ[%d:%s] dur %llu (s:%lld.%06lu,e:%lld.%06lu)\n",
					(int)b_isr->last_event,
					isr_name(b_isr->last_event),
					t_dur_isr,
					sec_high(b_isr->last_ts),
					sec_low(b_isr->last_ts),
					sec_high(b_isr->last_te),
					sec_low(b_isr->last_te));
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

static const char * const list_irq_disable[] = {
	/* debug purpose: sched_debug_show, sysrq_sched_debug_show */
	"print_cpu"
};

bool mt_dump_irq_off_traces(int mode)
{
	int i, j;
	char buf[128], func[64];
	struct stack_trace *trace;
	bool in_list = 0, skip_trace = 1;
	int list_num = ARRAY_SIZE(list_irq_disable);

	trace = &__raw_get_cpu_var(MT_stack_trace);
	list_num = ARRAY_SIZE(list_irq_disable);

	snprintf(buf, sizeof(buf),
		"irq off backtraces:");
	sched_mon_msg(buf, mode);
	for (i = 0; i < trace->nr_entries; i++) {

		/* skip dummy backtrace */
		snprintf(func, sizeof(func),
			"%ps", (void *)trace->entries[i]);
		if (!strcmp(func, "trace_hardirqs_off"))
			skip_trace = 0;
		if (skip_trace)
			continue;

		/* print backtrace */
		snprintf(buf, sizeof(buf),
			"[<%p>] %pS",
			(void *)trace->entries[i],
			(void *)trace->entries[i]);
		sched_mon_msg(buf, mode);

		/* check white list */
		for (j = 0; j < list_num; j++) {
			if (!strcmp(func, list_irq_disable[j])) {
				in_list = 1;
				snprintf(buf, sizeof(buf),
					"... %s is in white list\n", func);
				sched_mon_msg(buf, mode);
			}
		}
	}
	return in_list;
}

void MT_trace_hardirqs_on(void)
{
	unsigned long long t_on, t_off, t_dur;
	int output = TO_BOTH_SAVE;

	if (sched_mon_enable && (sched_mon_func_select & en_HARDIRQ)) {
#ifdef CONFIG_TRACE_IRQFLAGS
		if (current->hardirqs_enabled) {
			__raw_get_cpu_var(MT_tracing_cpu) = 0;
			return;
		}
#endif
		if (current->pid == 0) {	/* Ignore swap thread */
			__raw_get_cpu_var(MT_tracing_cpu) = 0;
			return;
		}
		if (__raw_get_cpu_var(MT_trace_in_sched))
			output = TO_FTRACE;

		if (__raw_get_cpu_var(MT_tracing_cpu) == 1) {
			MT_trace_irq_on();
			t_on = sched_clock();
			t_off = __raw_get_cpu_var(t_irq_off);
			t_dur = t_on - t_off;

			__raw_get_cpu_var(t_irq_on) = t_on;
			if (t_dur > WARN_IRQ_DISABLE_DUR) {
				char buf[144], buf2[64];
				bool skip_aee = 0;
				struct lock_block_event *b;

				sched_mon_evt[evt_HARDIRQ].warn_cnt++;
				snprintf(buf, sizeof(buf),
					"IRQ disable monitor: dur[%llu ms] off[%lld.%06lu] on[%lld.%06lu]",
					msec_high(t_dur),
					sec_high(t_off), sec_low(t_off),
					sec_high(t_on), sec_low(t_on));
				sched_mon_msg(buf, output);
#ifdef CONFIG_TRACE_IRQFLAGS
				snprintf(buf2, sizeof(buf2),
					"[<%p>] %ps",
					(void *)current->hardirq_disable_ip,
					(void *)current->hardirq_disable_ip);
				snprintf(buf, sizeof(buf),
					"hardirqs last disabled at %s", buf2);
				sched_mon_msg(buf, output);
#else
				snprintf(buf2, sizeof(buf2), "<no info>");
#endif
				b = &__raw_get_cpu_var(spinlock_mon);
				if (b->last_spinning_s > t_off) {
					snprintf(buf, sizeof(buf),
						"wait spinlock from [%lld.%06lu] to [%lld.%06lu]",
						sec_high(b->last_spinning_s),
						sec_low(b->last_spinning_s),
						sec_high(b->last_spinning_e),
						sec_low(b->last_spinning_e));
					sched_mon_msg(buf, output);
				}
				skip_aee = mt_dump_irq_off_traces(TO_FTRACE);

				if (t_dur > AEE_IRQ_DISABLE_DUR && !skip_aee) {
					mt_dump_irq_off_traces(TO_DEFERRED);
					snprintf(buf, sizeof(buf),
					"IRQ disable [%llu ms, %lld.%06lu ~ %lld.%06lu] at %s",
					msec_high(t_dur),
					sec_high(t_off), sec_low(t_off),
					sec_high(t_on), sec_low(t_on), buf2);
					sched_monitor_aee(evt_HARDIRQ, buf);
				}
			}
			__raw_get_cpu_var(t_irq_off) = 0;
		}
		__raw_get_cpu_var(MT_tracing_cpu) = 0;
	}
}
EXPORT_SYMBOL(MT_trace_hardirqs_on);

void MT_trace_hardirqs_off(void)
{
	if (sched_mon_enable && (sched_mon_func_select & en_HARDIRQ)) {
#ifdef CONFIG_TRACE_IRQFLAGS
		if (!current->hardirqs_enabled)
			return;
#endif
		if (current->pid == 0)	/* Ignore swap thread */
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
#if 0
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
#endif
}

static ssize_t mt_sched_monitor_write(struct file *filp, const char *ubuf,
				      size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val = 0;
	int ret;

	if (!sched_mon_door_key)
		return cnt;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	/* 0:off, 1:on */
	/* val = !!val; */
	if (val == 8)
		mt_dump_sched_traces();
#ifdef CONFIG_PREEMPT_MONITOR
	if (val == 18)		/* 0x12 */
		mt_dump_irq_off_traces(TO_KERNEL_LOG);
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

static ssize_t mt_sched_monitor_door_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[16];

	if (cnt >= sizeof(buf) || cnt <= 1UL)
		return cnt;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt-1UL] = 0;

	if (strcmp("open", buf) == 0)
		sched_mon_door_key = 1;
	if (strcmp("close", buf) == 0)
		sched_mon_door_key = 0;

	return cnt;
}

static const struct file_operations mt_sched_monitor_door_fops = {
	.open = simple_open,
	.write = mt_sched_monitor_door_write,
};

static int mt_sched_monitor_func_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "0x%x\n", sched_mon_func_select);
	return 0;
}

static int mt_sched_monitor_func_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_sched_monitor_func_show, NULL);
}

static ssize_t mt_sched_monitor_func_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int ret;

	if (!sched_mon_door_key)
		return cnt;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtouint(buf, 16, &sched_mon_func_select);
	if (ret)
		return ret;

	return cnt;
}

static const struct file_operations mt_sched_monitor_func_select_fops = {
	.open = mt_sched_monitor_func_open,
	.read = seq_read,
	.write = mt_sched_monitor_func_write,
	.llseek	= seq_lseek,
	.release = seq_release,
};

static int mt_sched_monitor_event_count_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < NUM_EVENTS; i++)
		SEQ_printf(m, "    %s \twarn [%8lu]  aee [%4lu]\n",
			sched_mon_evt[i].name,
			sched_mon_evt[i].warn_cnt,
			sched_mon_evt[i].aee_cnt);
	return 0;
}

static int mt_sched_monitor_event_count_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, mt_sched_monitor_event_count_show, NULL);
}

static const struct file_operations mt_sched_monitor_event_count_fops = {
	.open = mt_sched_monitor_event_count_open,
	.read = seq_read,
};

#define DECLARE_MT_SCHED_MATCH(param, warn_dur)			\
static ssize_t mt_sched_monitor_##param##_write(			\
	struct file *filp,					\
	const char *ubuf,				\
	size_t cnt, loff_t *data)				\
{									\
	char buf[64];							\
	unsigned long val = 0;						\
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

DECLARE_MT_SCHED_MATCH(SCHED_MON_ENABLE, sched_mon_enable);
DECLARE_MT_SCHED_MATCH(ISR_DUR, WARN_ISR_DUR);
DECLARE_MT_SCHED_MATCH(SOFTIRQ_DUR, WARN_SOFTIRQ_DUR);
DECLARE_MT_SCHED_MATCH(TASKLET_DUR, WARN_TASKLET_DUR);
DECLARE_MT_SCHED_MATCH(HRTIMER_DUR, WARN_HRTIMER_DUR);
DECLARE_MT_SCHED_MATCH(STIMER_DUR, WARN_STIMER_DUR);
DECLARE_MT_SCHED_MATCH(IRQ_WORK_DUR, WARN_IRQ_WORK_DUR);
DECLARE_MT_SCHED_MATCH(BURST_IRQ, WARN_BURST_IRQ_DETECT);
DECLARE_MT_SCHED_MATCH(PREEMPT_DUR, WARN_PREEMPT_DUR);
DECLARE_MT_SCHED_MATCH(IRQ_DISABLE_DUR, WARN_IRQ_DISABLE_DUR);
DECLARE_MT_SCHED_MATCH(IRQ_DISABLE_AEE_DUR, AEE_IRQ_DISABLE_DUR);
DECLARE_MT_SCHED_MATCH(COMMON_AEE_DUR, AEE_COMMON_DUR);
DECLARE_MT_SCHED_MATCH(IRQ_INFO_ENABLE, irq_info_enable);
DECLARE_MT_SCHED_MATCH(WARNING_ON, warning_on);
DECLARE_MT_SCHED_MATCH(AEE_ON_DURATION, aee_on_duration);
DECLARE_MT_SCHED_MATCH(AEE_ON_PREEMPT_BALANCE, aee_on_preempt_balance);
DECLARE_MT_SCHED_MATCH(AEE_ON_BURST_IRQ, aee_on_burst_irq);
DECLARE_MT_SCHED_MATCH(AEE_CNT_DURATION, aee_cnt_duration);
DECLARE_MT_SCHED_MATCH(AEE_CNT_PREEMPT_BALANCE, aee_cnt_preempt_balance);
DECLARE_MT_SCHED_MATCH(AEE_CNT_BURST_IRQ, aee_cnt_burst_irq);

enum sched_mon_dir_type {
	ROOT,
	DURATION,
	WARN_AEE,
	NUM_SCHED_MON_DIRS,
};
static struct proc_dir_entry *sched_mon_dir[NUM_SCHED_MON_DIRS];

struct sched_mon_proc_file {
	const char *name;
	enum sched_mon_dir_type dir;
	umode_t mode;
	const struct file_operations *proc_fops;
};
static struct sched_mon_proc_file sched_mon_file[] = {
	/* /proc/mtmon */
	{"sched_mon", ROOT, 0644,
		&mt_sched_monitor_fops},
	{"sched_mon_enable", ROOT, 0644,
		&mt_sched_monitor_SCHED_MON_ENABLE_fops},
	{"sched_mon_door", ROOT, 0220,
		&mt_sched_monitor_door_fops},
	{"sched_mon_event_count", ROOT, 0444,
		&mt_sched_monitor_event_count_fops},
	{"sched_mon_func_select", ROOT, 0664,
		&mt_sched_monitor_func_select_fops},
	{"irq_info_enable", ROOT, 0644,
		&mt_sched_monitor_IRQ_INFO_ENABLE_fops},

	/* /proc/mtmon/duration */
	{"sched_mon_duration_ISR", DURATION, 0644,
		&mt_sched_monitor_ISR_DUR_fops},
	{"sched_mon_duration_SOFTIRQ", DURATION, 0644,
		&mt_sched_monitor_SOFTIRQ_DUR_fops},
	{"sched_mon_duration_TASKLET", DURATION, 0644,
		&mt_sched_monitor_TASKLET_DUR_fops},
	{"sched_mon_duration_HRTIMER", DURATION, 0644,
		&mt_sched_monitor_HRTIMER_DUR_fops},
	{"sched_mon_duration_STIMER", DURATION, 0644,
		&mt_sched_monitor_STIMER_DUR_fops},
	{"sched_mon_duration_IRQ_WORK", DURATION, 0644,
		&mt_sched_monitor_IRQ_WORK_DUR_fops},
	{"sched_mon_duration_BURST_IRQ", DURATION, 0644,
		&mt_sched_monitor_BURST_IRQ_fops},
	{"sched_mon_duration_PREEMPT", DURATION, 0644,
		&mt_sched_monitor_PREEMPT_DUR_fops},
	{"sched_mon_duration_IRQ_DISABLE", DURATION, 0644,
		&mt_sched_monitor_IRQ_DISABLE_DUR_fops},
	{"sched_mon_duration_IRQ_DISABLE_AEE", DURATION, 0644,
		&mt_sched_monitor_IRQ_DISABLE_AEE_DUR_fops},
	{"sched_mon_duration_COMMON_AEE", DURATION, 0644,
		&mt_sched_monitor_COMMON_AEE_DUR_fops},

	/* /proc/mtmon/warn_aee */
	{"warning_on", WARN_AEE, 0644,
		&mt_sched_monitor_WARNING_ON_fops},
	{"aee_on_duration", WARN_AEE, 0644,
		&mt_sched_monitor_AEE_ON_DURATION_fops},
	{"aee_cnt_duration", WARN_AEE, 0644,
		&mt_sched_monitor_AEE_CNT_DURATION_fops},
	{"aee_on_preempt_balance", WARN_AEE, 0644,
		&mt_sched_monitor_AEE_ON_PREEMPT_BALANCE_fops},
	{"aee_cnt_preempt_balance", WARN_AEE, 0644,
		&mt_sched_monitor_AEE_CNT_PREEMPT_BALANCE_fops},
	{"aee_on_burst_irq", WARN_AEE, 0644,
		&mt_sched_monitor_AEE_ON_BURST_IRQ_fops},
	{"aee_cnt_burst_irq", WARN_AEE, 0644,
		&mt_sched_monitor_AEE_CNT_BURST_IRQ_fops},
};

static int __init init_mtsched_mon(void)
{
	int cpu, i;
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
		per_cpu(irq_work_mon, cpu).type = evt_IRQWORK;
		per_cpu(RCU_SoftIRQ_mon, cpu).type = RCU_SOFTIRQ;
	}

	sched_mon_dir[ROOT] = proc_mkdir("mtmon", NULL);
	if (!sched_mon_dir[ROOT])
		return -1;
	sched_mon_dir[DURATION] = proc_mkdir("duration", sched_mon_dir[ROOT]);
	if (!sched_mon_dir[DURATION])
		return -1;
	sched_mon_dir[WARN_AEE] = proc_mkdir("warn_aee", sched_mon_dir[ROOT]);
	if (!sched_mon_dir[WARN_AEE])
		return -1;

	for (i = 0; i < ARRAY_SIZE(sched_mon_file); i++) {
		pe = proc_create(sched_mon_file[i].name,
			sched_mon_file[i].mode,
			sched_mon_dir[sched_mon_file[i].dir],
			sched_mon_file[i].proc_fops);
		if (!pe) {
			pr_info("create [%s] failed\n", sched_mon_file[i].name);
			return -ENOMEM;
		}
	}

	mt_sched_monitor_test_init(sched_mon_dir[ROOT]);

	sched_mon_enable = 1;
	sched_mon_func_select = en_HARDIRQ;
	warning_on = 0;
	aee_on_duration = 0;
	aee_cnt_duration = 3;
	aee_on_preempt_balance = 1;
	aee_cnt_preempt_balance = 1;
	aee_on_burst_irq = 1;
	aee_cnt_burst_irq = 1;

	return 0;
}

device_initcall(init_mtsched_mon);
