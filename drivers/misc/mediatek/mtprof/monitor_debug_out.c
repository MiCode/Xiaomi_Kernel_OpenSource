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

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/pid.h>

#include "mt_sched_mon.h"

#include <mt-plat/aee.h>
#include <linux/stacktrace.h>
#include "internal.h"

#ifdef CONFIG_MT_SCHED_MONITOR

#define WDT_SCHED_MON_LOG_SIZE	120
static char wdt_sched_mon_log_buf[WDT_SCHED_MON_LOG_SIZE];

#define get_sched_block_events(isr, softirq, tasklet, hrtimer, soft_timer) \
	do {	\
		isr = &per_cpu(ISR_mon, cpu);	\
		softirq = &per_cpu(SoftIRQ_mon, cpu);	\
		tasklet = &per_cpu(tasklet_mon, cpu);	\
		hrtimer = &per_cpu(hrt_mon, cpu);	\
		soft_timer = &per_cpu(sft_mon, cpu); \
	} while (0)

#define get_sched_stop_events(preempt_disable_event, irq_disable_event)	\
	do {	\
		preempt_disable_event = &per_cpu(Preempt_disable_mon, cpu);	\
		irq_disable_event = &per_cpu(IRQ_disable_mon, cpu);	\
	} while (0)

#include <linux/irqnr.h>
#include <linux/kernel_stat.h>
#include <asm/hardirq.h>

void mt_show_last_irq_counts(void)
{
	int irq, cpu;
	unsigned long flags;

	spin_lock_irqsave(&mt_irq_count_lock, flags);
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		pr_err("Last irq counts record at [%llu] ns\n", per_cpu(save_irq_count_time, cpu));
		pr_err("CPU%d state:%s\n", cpu, cpu_online(cpu) ? "online" : "offline");
		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			if (per_cpu(irq_count_mon, cpu).irqs[irq] != 0)
				pr_err("[%3d] = %8d\n", irq, per_cpu(irq_count_mon, cpu).irqs[irq]);
		}
	}

#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		for (irq = 0; irq < NR_IPI; irq++) {
			if (per_cpu(ipi_count_mon, cpu).ipis[irq] != 0)
				pr_err("(CPU#%d)IPI[%3d] = %8d\n", cpu, irq,
				       per_cpu(ipi_count_mon, cpu).ipis[irq]);
		}
	}
#endif
	spin_unlock_irqrestore(&mt_irq_count_lock, flags);
}

void mt_show_current_irq_counts(void)
{
	int irq, cpu, count;
	unsigned long flags;
	unsigned long long t_cur, t_diff = 0;

	t_cur = sched_clock();
	spin_lock_irqsave(&mt_irq_count_lock, flags);

	pr_err("=========================================\nIRQ Status:\n");
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		t_diff = t_cur - per_cpu(save_irq_count_time, cpu);
		pr_err("Current irq counts record at [%llu] ns.(last at %llu, diff:+%llu ns)\n", t_cur,
	       per_cpu(save_irq_count_time, cpu), usec_high(t_diff));
		pr_err("CPU%d state:%s\n", cpu, cpu_online(cpu) ? "online" : "offline");
		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			count = kstat_irqs_cpu(irq, cpu);
			if (count != 0)
				pr_err(" IRQ[%3d:%14s] = %8d, (+%d times in %lld us)\n", irq,
				       isr_name(irq), count, count - per_cpu(irq_count_mon, cpu).irqs[irq],
				       usec_high(t_diff));
		}
	}
#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		pr_err("Local IRQ on CPU#%d:\n", cpu);
		for (irq = 0; irq < NR_IPI; irq++) {
			count = __get_irq_stat(cpu, ipi_irqs[irq]);
			if (count != 0)
				pr_err(" IRQ[%2d:  IPI] = %8d,(+%d times in %lld us)\n", irq, count,
				       count - per_cpu(ipi_count_mon, cpu).ipis[irq], usec_high(t_diff));
		}
	}
#endif
	spin_unlock_irqrestore(&mt_irq_count_lock, flags);
}

void mt_show_timer_info(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		pr_err("[TimerISR#%d] last timer ISR start:%llu ns, end:%llu ns\n", cpu,
		       per_cpu(local_timer_ts, cpu), per_cpu(local_timer_te, cpu));
	}
}

/* for MTK fiq debug log mechanism*/
static void mt_aee_show_current_irq_counts(void)
{

	int irq, cpu, count;
	unsigned long long t_cur, t_diff;

	t_cur = sched_clock();
	/* spin_lock_irqsave(&mt_irq_count_lock, flags); */

	snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf), "\nIRQ Status\n");
	aee_sram_fiq_log(wdt_sched_mon_log_buf);
	memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		t_diff = t_cur - per_cpu(save_irq_count_time, cpu);
		snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf), "Dur:%lld us,(now:%lld,last:%lld)\n",
			usec_high(t_diff), usec_high(t_cur), usec_high(per_cpu(save_irq_count_time, cpu)));
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
		memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));

		snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf), "CPU%d state:%s\n",
			cpu, cpu_online(cpu) ? "online" : "offline");
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
		memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));

		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			count = kstat_irqs_cpu(irq, cpu);
			if (count != 0) {
				snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf), " %d:%s +%d(%d)\n",
					irq, isr_name(irq), count - per_cpu(irq_count_mon, cpu).irqs[irq], count);
				aee_sram_fiq_log(wdt_sched_mon_log_buf);
				memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
			}
		}
	}
#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf), "CPU#%d:\n", cpu);
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
		memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		for (irq = 0; irq < NR_IPI; irq++) {
			count = __get_irq_stat(cpu, ipi_irqs[irq]);
			if (count != 0) {
				snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf), " %d:IPI +%d(%d)\n",
					irq, count - per_cpu(ipi_count_mon, cpu).ipis[irq], count);
				aee_sram_fiq_log(wdt_sched_mon_log_buf);
				memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
			}
		}
	}
#endif
	/* spin_unlock_irqrestore(&mt_irq_count_lock, flags); */
}

static void mt_aee_show_timer_info(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf), "[TimerISR#%d]last s:%llu e:%llu ns\n",
			cpu, per_cpu(local_timer_ts, cpu), per_cpu(local_timer_te, cpu));
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
		memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
	}
}

void mt_dump_sched_traces(void)
{
	struct sched_block_event *b_isr, *b_sq, *b_tk, *b_hrt, *b_sft;
	struct sched_stop_event *e_irq, *e_pmpt;
	int cpu;

	for_each_possible_cpu(cpu) {
		get_sched_block_events(b_isr, b_sq, b_tk, b_hrt, b_sft);
		get_sched_stop_events(e_pmpt, e_irq);
		pr_err("==CPU[%d]==\n", cpu);
		if (b_isr->cur_event == 0)
			pr_err("[ISR]last_irq:%d, dur:%llu ns (s:%llu, e:%llu)\n\n",
					(int)b_isr->last_event, b_isr->last_te - b_isr->last_ts,
					b_isr->last_ts, b_isr->last_te);
		else {
			pr_err("[In ISR]Current irq:%d, Start:%llu (elapsed: %llu ns),",
				(int)b_isr->cur_event, b_isr->cur_ts,
				sched_clock() - b_isr->cur_ts);
			pr_err(" last irq#:%d, last_start:%llu, last_end:%llu\n\n",
				(int)b_isr->last_event, b_isr->last_ts, b_isr->last_te);
		}

		if (b_sq->cur_event == 0)
			pr_err("[Softirq]last#:%d, dur:%llu ns (s:%llu, e:%llu)\n\n",
					(int)b_sq->last_event, b_sq->last_te - b_sq->last_ts,
					b_sq->last_ts, b_sq->last_te);
		else {
		    pr_err("[In Softirq]Current softirq#:%d, Start:%llu(elapsed: %llu ns),",
				(int)b_sq->cur_event, b_sq->cur_ts,
				sched_clock() - b_sq->cur_ts);
		    pr_err(" last softirq#:%d(dur:%llu ns), last_start:%llu, last_end:%llu\n\n",
				(int)b_sq->last_event, b_sq->last_te - b_sq->last_ts,
				b_sq->last_ts, b_sq->last_te);
		}

		if (b_tk->cur_event == 0) {
			pr_err("[Tasklet]\n Occurs %d times in last SoftIRQ duration\n",
					(int)b_tk->last_count);
			pr_err("last fn:%pS, dur:%llu ns (s:%llu, e:%llu)\n\n",
				(void *)b_tk->last_event, b_tk->last_te - b_tk->last_ts,
				b_tk->last_ts, b_tk->last_te);
		} else {
		    pr_err("[In Tasklet]\n Occurs: cur:%d, last:%d\n Current:%pS,",
				(int)b_tk->cur_count, (int)b_tk->last_count,
				(void *)b_tk->cur_event);
			pr_err(" Start:%llu(elapsed: %llu ns), last#:%pS(dur:%llu ns),",
				b_tk->cur_ts, sched_clock() - b_tk->cur_ts,
				(void *)b_tk->last_event, b_tk->last_te - b_tk->last_ts);
			pr_err(" last_start:%llu, last_end:%llu\n\n",
				b_tk->last_ts, b_tk->last_te);
		}

		if (b_hrt->cur_event == 0) {
			pr_err("[HRTimer]\n Occurs %d times in last ISR duration\n",
					(int)b_hrt->last_count);
			pr_err(" last fn:%pS, dur:%llu ns (s:%llu, e:%llu)\n\n",
					(void *)b_hrt->last_event,
					b_hrt->last_te - b_hrt->last_ts,
					b_hrt->last_ts, b_hrt->last_te);
		} else {
		    pr_err("[In HRTimer]\n Occurs: cur:%d, last:%d\n Current:%pS, ",
				(int)b_tk->cur_count, (int)b_tk->last_count,
				(void *)b_hrt->cur_event);
		    pr_err("Start:%llu(elapsed: %llu ns), last#:%pS(dur:%llu ns), ",
				b_hrt->cur_ts, sched_clock() - b_hrt->cur_ts,
				(void *)b_hrt->last_event, b_hrt->last_te - b_hrt->last_ts);
		    pr_err("last_start:%llu, last_end:%llu\n\n",
				b_hrt->last_ts, b_hrt->last_te);
		}

		if (b_sft->cur_event == 0) {
			pr_err("[SoftTimer]\n Occurs %d times in last SoftIRQ duration\n",
					(int)b_sft->last_count);
		    pr_err(" last fn:%pS, dur:%llu ns (s:%llu, e:%llu)\n\n",
				(void *)b_sft->last_event,
				b_sft->last_te - b_sft->last_ts,
				b_sft->last_ts, b_sft->last_te);
		} else {
		    pr_err("[In SoftTimer]\n Occurs: cur:%d, last:%d\n Current:%pS,",
				(int)b_sft->cur_count, (int)b_sft->last_count,
				(void *)b_sft->cur_event);
		    pr_err(" Start:%llu(elapsed: %llu ns), last#:%pS(dur:%llu ns), ",
				b_sft->cur_ts, sched_clock() - b_sft->cur_ts,
				(void *)b_sft->last_event, b_sft->last_te - b_sft->last_ts);
		    pr_err("last_start:%llu, last_end:%llu\n\n",
				b_sft->last_ts, b_sft->last_te);
		}

/****  Dump Stop Events ****/
		if (e_irq->cur_ts == 0)
				pr_err("[IRQ disable] last duration:%llu ns (s: %llu, e: %llu)\n\n",
					e_irq->last_te - e_irq->last_ts, e_irq->last_ts, e_irq->last_te);
		else
		    pr_err
		    ("[IRQ disable] cur_ts:%llu(elapsed:%llu ns), last duration:%llu ns(s: %llu, e: %llu)\n\n",
		     e_irq->cur_ts, sched_clock() - e_irq->cur_ts,
		     e_irq->last_te - e_irq->last_ts,
		     e_irq->last_ts, e_irq->last_te);


		if (e_pmpt->cur_ts == 0)
			pr_err("[Preempt disable] last duration:%llu ns(s: %llu, e: %llu)\n\n",
				e_pmpt->last_te - e_pmpt->last_ts,
				e_pmpt->last_ts, e_pmpt->last_te);
		else
		    pr_err
		    ("[Preempt disable] cur_ts:%llu(elapsed:%llu ns), last duration:%llu ns(s: %llu, e: %llu)\n\n",
		     e_pmpt->cur_ts, sched_clock() - e_pmpt->cur_ts,
		     e_pmpt->last_te - e_pmpt->last_ts, e_pmpt->last_ts, e_pmpt->last_te);
	}
	mt_show_current_irq_counts();
	mt_show_timer_info();
}

void mt_aee_dump_sched_traces(void)
{
	struct sched_block_event *b_isr, *b_sq, *b_tk, *b_hrt, *b_sft;
	struct sched_stop_event *e_irq, *e_pmpt;
	int cpu;

	for_each_possible_cpu(cpu) {
		get_sched_block_events(b_isr, b_sq, b_tk, b_hrt, b_sft);
		get_sched_stop_events(e_pmpt, e_irq);

		snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf), "CPU%d\n", cpu);
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
		memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));

		if (b_isr->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[ISR]last#%d,dur:%lld s:%lld\n\n",
				(int)b_isr->last_event,
				usec_high(b_isr->last_te - b_isr->last_ts),
				usec_high(b_isr->last_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		} else {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[In ISR]Current irq#:%d, Start:%lld (elapsed: %lld), last irq#:%d, s:%lld, e:%lld\n\n",
				(int)b_isr->cur_event, usec_high(b_isr->cur_ts),
				usec_high(sched_clock() - b_isr->cur_ts), (int)b_isr->last_event,
				usec_high(b_isr->last_ts), usec_high(b_isr->last_te));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		}

		if (b_sq->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[Softirq]last#%d,dur:%lld s:%lld\n\n",
				(int)b_sq->last_event, usec_high(b_sq->last_te - b_sq->last_ts),
				usec_high(b_sq->last_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		} else {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[In Softirq]Current softirq#:%d, Start:%lld(elapsed: %lld),",
				(int)b_sq->cur_event, usec_high(b_sq->cur_ts),
				usec_high(sched_clock() - b_sq->cur_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				" last softirq#:%d(dur:%lld), s:%lld, e:%lld\n\n",
				(int)b_sq->last_event,
				usec_high(b_sq->last_te - b_sq->last_ts),
				usec_high(b_sq->last_ts), usec_high(b_sq->last_te));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		}

		if (b_tk->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[Tasklet]%d/SoftIRQ\n %pS dur:%lld s:%lld\n\n",
				(int)b_tk->last_count, (void *)b_tk->last_event,
				usec_high(b_tk->last_te - b_tk->last_ts),
				usec_high(b_tk->last_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		} else {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[In Tasklet]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%lld(elapsed: %lld),",
				(int)b_tk->cur_count, (int)b_tk->last_count, (void *)b_tk->cur_event,
				usec_high(b_tk->cur_ts), usec_high(sched_clock() - b_tk->cur_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				" last#:%pS(dur:%lld), last_start:%lld, last_end:%lld\n\n",
				(void *)b_tk->last_event, usec_high(b_tk->last_te - b_tk->last_ts),
				usec_high(b_tk->last_ts), usec_high(b_tk->last_te));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		}

		if (b_hrt->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[HRTimer]%d/ISR\n %pS dur:%lld s:%lld\n\n",
				   (int)b_hrt->last_count, (void *)b_hrt->last_event,
				   usec_high(b_hrt->last_te - b_hrt->last_ts),
				   usec_high(b_hrt->last_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		} else {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[In HRTimer]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%lld(elapsed: %lld),",
				(int)b_tk->cur_count, (int)b_tk->last_count, (void *)b_hrt->cur_event,
				usec_high(b_hrt->cur_ts), usec_high(sched_clock() - b_hrt->cur_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				" last#:%pS(dur:%lld), last_start:%lld, last_end:%lld\n\n",
				(void *)b_hrt->last_event, usec_high(b_hrt->last_te - b_hrt->last_ts),
				usec_high(b_hrt->last_ts), usec_high(b_hrt->last_te));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		}

		if (b_sft->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[SoftTimer]%d/SoftIRQ\n %pS dur:%lld s:%lld\n\n",
				(int)b_sft->last_count, (void *)b_sft->last_event,
				usec_high(b_sft->last_te - b_sft->last_ts),
				usec_high(b_sft->last_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		} else {
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				"[In SoftTimer]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%lld(elapsed: %lld),",
				(int)b_sft->cur_count, (int)b_sft->last_count, (void *)b_sft->cur_event,
				usec_high(b_sft->cur_ts), usec_high(sched_clock() - b_sft->cur_ts));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
			snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
				" last#:%pS(dur:%lld), last_start:%lld, last_end:%lld\n\n",
				(void *)b_sft->last_event, usec_high(b_sft->last_te - b_sft->last_ts),
				usec_high(b_sft->last_ts), usec_high(b_sft->last_te));
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
			memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));
		}
		/****  Dump Stop Events ****/
		/*
		 * e_irq->cur_ts == 0?
		   aee_wdt_printf("[IRQ disable] last duration:%llu ns (s: %llu, e: %llu)\n\n",
		   e_irq->last_te - e_irq->last_ts, e_irq->last_ts, e_irq->last_te):
		   aee_wdt_printf("[IRQ disable] cur_ts:%llu(elapsed:%llu ns), "
		   "last duration:%llu ns(s: %llu, e: %llu)\n\n",
		   e_irq->cur_ts, sched_clock() - e_irq->cur_ts, e_irq->last_te - e_irq->last_ts,"
		   " e_irq->last_ts, e_irq->last_te);

		   e_pmpt->cur_ts == 0?
		   aee_wdt_printf("[Preempt disable] last duration:%llu ns(s: %llu, e: %llu)\n\n",
		   e_pmpt->last_te - e_pmpt->last_ts, e_pmpt->last_ts, e_pmpt->last_te):
		   aee_wdt_printf("[Preempt disable] cur_ts:%llu(elapsed:%llu ns), "
		   "last duration:%llu ns(s: %llu, e: %llu)\n\n",
		   e_pmpt->cur_ts, sched_clock() - e_pmpt->cur_ts, e_pmpt->last_te - e_pmpt->last_ts,"
		   " e_pmpt->last_ts, e_pmpt->last_te);
		 */
	}
	mt_aee_show_current_irq_counts();
	mt_aee_show_timer_info();
}

#else

void mt_dump_sched_traces(void)
{
}

void mt_aee_dump_sched_traces(void)
{
}

void mt_show_last_irq_counts(void)
{
}

void mt_show_current_irq_counts(void)
{
}

void mt_show_timer_info(void)
{
}
#endif
