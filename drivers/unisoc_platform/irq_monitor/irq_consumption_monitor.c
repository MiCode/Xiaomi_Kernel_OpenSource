// SPDX-License-Identifier: GPL-2.0-only
/* copyright (C) 2023 Unisoc (Shanghai) Technologies Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <asm/irq.h>
#include <linux/kprobes.h>
#include <linux/sched/clock.h>
#include <linux/math64.h>
#include <trace/events/irq.h>
#define CREATE_TRACE_POINTS

#if IS_ENABLED(CONFIG_IRQ_CONSUMPTION_MONITOR)

#define ONE_SEC_IN_NS	1000000000
#define CPU_CONSUMPTION_THRESHOLD_RATE	30
#define CPU_CONSUMPTION_THRESHOLD_RATE_PER_IRQ	20
#define IRQ_COUNT	512

/**
 * struct irq_consumption_stat - single irq's cpu consumption stastic
 * @win_start: start time of statistic window
 * @irq_start: time of irq entry
 * @whole_irq_dur_in_win: this irq's duration for this stat window
 */
struct irq_consumption_stat {
	unsigned long long	win_start;
	unsigned long long	win_count;
	unsigned long long	whole_irq_dur_in_win;
} ____cacheline_internodealigned_in_smp;

static DEFINE_PER_CPU(unsigned long long [IRQ_COUNT], per_irq_start);
static DEFINE_PER_CPU(struct irq_consumption_stat, per_irq_stat);

/**
 * struct irq_timing_record - all irqs consumption recorder
 * @irq_dur_in_win: all irq's duration for this stat window
 * @win_count: stat window count
 */
struct irq_timing_record {
	unsigned long long	irq_dur_in_win;
	unsigned long long	win_count;
} ____cacheline_internodealigned_in_smp;

static DEFINE_PER_CPU(struct irq_timing_record [IRQ_COUNT], irq_timing_rec_array);

/**
 * per-instance private data
 */
struct my_data {
	int entry_stamp;
};

static void trace_irqentry_callback(void *data, int irq,
				    struct irqaction *action)
{
	int cpu = smp_processor_id();

	per_cpu(per_irq_start, cpu)[irq] = sched_clock();
}

static u64 do_the_div(u64 dividend, u64 divisor)
{
	u64 tmp;

	return div64_u64_rem(dividend, divisor, &tmp);
}

static void trace_irqexit_callback(void *data, int irq,
				   struct irqaction *action, int ret)
{
	int cpu = smp_processor_id();
	unsigned long long curr_time = sched_clock();
	unsigned long long dur_this_irq;
	unsigned long long real_win = 0;
	unsigned long long cpu_consumption_rate = 0;
	unsigned long long cpu_consumption_rate_this_irq = 0;
	unsigned long long dur_in_us = 0;
	unsigned long long win_in_us = 0;
	struct irq_timing_record *p_irq_t_rec;
	struct irq_consumption_stat *p_ic_stat;
	struct irq_desc *desc;

	dur_this_irq = curr_time - per_cpu(per_irq_start, cpu)[irq];
	p_irq_t_rec = &per_cpu(irq_timing_rec_array, cpu)[irq];
	p_irq_t_rec->irq_dur_in_win += dur_this_irq;

	p_ic_stat = &per_cpu(per_irq_stat, cpu);
	p_ic_stat->whole_irq_dur_in_win += dur_this_irq;

	real_win = curr_time - p_ic_stat->win_start;
	if (real_win >= ONE_SEC_IN_NS) {
		cpu_consumption_rate_this_irq =
			do_the_div(p_irq_t_rec->irq_dur_in_win * 100, real_win);
		if (cpu_consumption_rate_this_irq > CPU_CONSUMPTION_THRESHOLD_RATE_PER_IRQ) {
			desc = irq_to_desc(irq);
			if (desc) {
				dur_in_us = do_the_div(p_irq_t_rec->irq_dur_in_win, NSEC_PER_USEC);
				win_in_us = do_the_div(real_win, NSEC_PER_USEC);
				pr_warn("CWARN: cpu=%d, single irq consumption rate=%lld percents\n",
						cpu, cpu_consumption_rate_this_irq);
				pr_warn("CWARN: cpu=%d, irq %d:%d:%s, con:%lld us, dur:%lld us\n",
						cpu, irq, desc->irq_data.hwirq, desc->action->name,
						dur_in_us, win_in_us);
			}
		}

		cpu_consumption_rate =
			do_the_div(p_ic_stat->whole_irq_dur_in_win * 100, real_win);
		if (cpu_consumption_rate > CPU_CONSUMPTION_THRESHOLD_RATE) {
			desc = irq_to_desc(irq);
			if (desc) {
				dur_in_us = do_the_div(p_ic_stat->whole_irq_dur_in_win,
							NSEC_PER_USEC);
				win_in_us = do_the_div(real_win, NSEC_PER_USEC);
				pr_warn("CWARN: cpu=%d, whole irq consumption rate=%lld percents\n",
						cpu, cpu_consumption_rate);
				pr_warn("CWARN: cpu=%d, cur_irq %d:%d:%s, con:%lld us, dur:%lld us\n",
						cpu, irq, desc->irq_data.hwirq, desc->action->name,
						dur_in_us, win_in_us);
			}
		}

		p_ic_stat->win_start = curr_time;
		p_ic_stat->whole_irq_dur_in_win = 0;
		p_ic_stat->win_count += 1;

		p_irq_t_rec->win_count = p_ic_stat->win_count;
		p_irq_t_rec->irq_dur_in_win = 0;
	}

	if (p_irq_t_rec->win_count != p_ic_stat->win_count) {
		p_irq_t_rec->win_count = p_ic_stat->win_count;
		p_irq_t_rec->irq_dur_in_win = 0;
	}
}

void consumption_monitor_init(void)
{
	int ret;
	int i;
	unsigned int cpu;

	ret = register_trace_irq_handler_entry(trace_irqentry_callback, NULL);
	if (ret)
		goto out_err;

	pr_info("CON_INFO: register trace_irqentry_callback succeeded!\n");

	ret = register_trace_irq_handler_exit(trace_irqexit_callback, NULL);
	if (ret)
		goto out_unregister_entry;

	pr_info("CON_INFO: register trace_irqexit_callback succeeded!\n");

	for_each_possible_cpu(cpu) {
		per_cpu(per_irq_stat, cpu).win_start = sched_clock();
		per_cpu(per_irq_stat, cpu).win_count = 0x0;
		per_cpu(per_irq_stat, cpu).whole_irq_dur_in_win = 0x0;

		for (i = 0; i < IRQ_COUNT; i++) {
			per_cpu(irq_timing_rec_array, cpu)[i].irq_dur_in_win = 0x0;
			per_cpu(irq_timing_rec_array, cpu)[i].win_count = 0x0;
		}
	}

	return;

out_unregister_entry:
	unregister_trace_irq_handler_entry(trace_irqentry_callback, NULL);
out_err:
	return;
}

void consumption_monitor_exit(void)
{
	unregister_trace_irq_handler_exit(trace_irqexit_callback, NULL);
	unregister_trace_irq_handler_entry(trace_irqentry_callback, NULL);
}

#endif
