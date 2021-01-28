/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef _MTK_SCHED_MON_H
#define _MTK_SCHED_MON_H

#include <asm/hardirq.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>
#endif

#ifndef TO_KERNEL_LOG
#define TO_KERNEL_LOG  0x1
#define TO_FTRACE      0x2
#define TO_DEFERRED    0x4
#define TO_SRAM        0x8
#define TO_BOTH       (TO_KERNEL_LOG | TO_FTRACE)
#define TO_BOTH_SAVE  (TO_DEFERRED | TO_FTRACE)
#endif

void sched_mon_msg(int out, char *buf, ...);

#ifdef CONFIG_MTK_SCHED_MONITOR
extern bool irq_time_tracer;
extern unsigned int irq_time_th1_ms;
extern unsigned int irq_time_th2_ms;
extern unsigned int irq_time_aee_limit;

struct irq_handle_status {
	unsigned int irq;
	unsigned long long start;
	unsigned long long end;
};

DECLARE_PER_CPU(struct irq_handle_status, irq_note);
DECLARE_PER_CPU(struct irq_handle_status, ipi_note);
DECLARE_PER_CPU(struct irq_handle_status, softirq_note);

#ifdef CONFIG_MTK_AEE_FEATURE
#define schedule_monitor_aee(msg, item) do { \
	char aee_str[64]; \
					\
	snprintf(aee_str, sizeof(aee_str), \
		"SCHED MONITOR: %s", item); \
	RCU_NONIDLE(aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE, \
		aee_str, "\n%s\n", msg)); \
} while (0)
#else
#define schedule_monitor_aee(msg_str, item) do {} while (0)
#endif

#define WARN_INFO ": process time %llums, from %ld.%06ld to %ld.%06ld"

#define check_start_time(time) do { \
	if (irq_time_tracer) \
		time = sched_clock(); \
} while (0)

#define __check_process_time(msg, _ts, ...) do { \
	unsigned long long ts = _ts; \
	unsigned long long te = sched_clock(); \
	unsigned long long t_diff = te - ts; \
					\
	do_div(t_diff, 1000000); \
	if (t_diff > irq_time_th1_ms) { \
		struct timeval tv_start, tv_end; \
		char msg_str[128]; \
					\
		tv_start = ns_to_timeval(ts); \
		tv_end = ns_to_timeval(te); \
		snprintf(msg_str, sizeof(msg_str), \
			 msg WARN_INFO, ##__VA_ARGS__, t_diff, \
			 tv_start.tv_sec, tv_start.tv_usec, \
			 tv_end.tv_sec, tv_end.tv_usec); \
		sched_mon_msg(TO_BOTH, msg_str); \
	} \
} while (0)

#define check_process_time(msg, ts, ...) do { \
	if (irq_time_tracer) \
		__check_process_time(msg, ts, ##__VA_ARGS__); \
} while (0)

#define check_preempt_count(_count, msg, ...) do { \
	int count = _count; \
	\
	if (count != preempt_count()) { \
		char msg_str[128]; \
				\
		snprintf(msg_str, sizeof(msg_str), "entered " msg \
			 " with preempt_count %08x, exited with %08x", \
			 ##__VA_ARGS__, count, preempt_count()); \
		sched_mon_msg(TO_BOTH, msg_str); \
		schedule_monitor_aee(msg_str, "PREEMPT COUNT"); \
	} \
} while (0)

#define check_start_time_preempt(type, count, time, id) do { \
	if (irq_time_tracer) { \
		time = sched_clock(); \
		this_cpu_write(type.irq, id); \
		this_cpu_write(type.start, time); \
		this_cpu_write(type.end, 0); \
		count = preempt_count(); \
	} \
} while (0)

#define __check_process_time_preempt(type, count, msg, _ts, ...) do { \
	unsigned long long ts = _ts; \
	unsigned long long te = sched_clock(); \
	unsigned long long t_diff = te - ts; \
					\
	do_div(t_diff, 1000000); \
	if (t_diff > irq_time_th1_ms) { \
		struct timeval tv_start, tv_end; \
		char msg_str[128]; \
					\
		tv_start = ns_to_timeval(ts); \
		tv_end = ns_to_timeval(te); \
		snprintf(msg_str, sizeof(msg_str), \
			 msg WARN_INFO, ##__VA_ARGS__, t_diff, \
			 tv_start.tv_sec, tv_start.tv_usec, \
			 tv_end.tv_sec, tv_end.tv_usec); \
		sched_mon_msg(TO_BOTH, msg_str); \
		if (irq_time_th2_ms && irq_time_aee_limit && \
		    t_diff > irq_time_th2_ms) { \
			irq_time_aee_limit--; \
			schedule_monitor_aee(msg_str, \
				"IRQ PROCESSING TIME"); \
		} \
	} \
	this_cpu_write(type.end, te); \
	check_preempt_count(count, msg, ##__VA_ARGS__); \
} while (0)

#define check_process_time_preempt(type, count, msg, ts, ...) do { \
	if (irq_time_tracer) \
		__check_process_time_preempt(type, count, msg, ts, \
					     ##__VA_ARGS__); \
} while (0)

#ifdef CONFIG_MTK_RAM_CONSOLE
#define pr_aee_sram(msg) aee_sram_fiq_log(msg)
#else
#define pr_aee_sram(msg) do {} while (0)
#endif

void show_irq_handle_info(int output);
#ifdef CONFIG_MTK_IRQ_COUNT_TRACER
void show_irq_count_info(int output);
#else
#define show_irq_count_info(output) do {} while (0)
#endif

#ifdef CONFIG_MTK_IRQ_OFF_TRACER
void trace_hardirqs_off_time(void);
void trace_hardirqs_on_time(void);
#else
#define trace_hardirqs_off_time() do {} while (0)
#define trace_hardirqs_on_time() do {} while (0)
#endif

#ifdef CONFIG_MTK_PREEMPT_TRACER
void trace_preempt_off_time(void);
void trace_preempt_on_time(void);
#else
#define trace_preempt_off_time() do {} while (0)
#define trace_preempt_on_time() do {} while (0)
#endif

void mt_aee_dump_sched_traces(void);

#else /* !CONFIG_MTK_SCHED_MONITOR */
#define check_start_time(time) ((void)time)
#define check_process_time(msg, ts, ...) do {} while (0)
#define check_start_time_preempt(type, count, time, irq_id) do { \
	(void)time; \
	(void)count; \
} while (0)
#define check_process_time_preempt(type, count, msg, ts, ...) do {} while (0)
#define trace_hardirqs_off_time() do {} while (0)
#define trace_hardirqs_on_time() do {} while (0)
#define trace_preempt_off_time() do {} while (0)
#define trace_preempt_on_time() do {} while (0)
#define show_irq_handle_info(output) do {} while (0)
#define show_irq_count_info(output) do {} while (0)
#define mt_aee_dump_sched_traces() do {} while (0)
#endif /* CONFIG_MTK_SCHED_MONITOR */

const char *irq_to_name(int irq);

#endif /* _MTK_SCHED_MON_H */
