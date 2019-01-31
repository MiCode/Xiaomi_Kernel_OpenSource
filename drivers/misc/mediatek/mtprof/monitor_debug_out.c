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

#include "mtk_sched_mon.h"

#include <mt-plat/aee.h>
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>
#endif

#include <linux/stacktrace.h>
#include "internal.h"

#ifdef CONFIG_MTK_SCHED_MONITOR

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
		preempt_disable_event = &per_cpu(Preempt_disable_mon, cpu); \
		irq_disable_event = &per_cpu(IRQ_disable_mon, cpu); \
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
		pr_info("Last irq counts record at [%llu] ns\n",
			per_cpu(save_irq_count_time, cpu));
		pr_info("CPU%d state:%s\n",
			cpu, cpu_online(cpu) ? "online" : "offline");
		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			if (per_cpu(irq_count_mon, cpu).irqs[irq] != 0)
				pr_info("[%3d] = %8d\n", irq,
					per_cpu(irq_count_mon, cpu).irqs[irq]);
		}
	}

#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		for (irq = 0; irq < NR_IPI; irq++) {
			if (per_cpu(ipi_count_mon, cpu).ipis[irq] != 0)
				pr_info("(CPU#%d)IPI[%3d] = %8d\n", cpu, irq,
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

	pr_info("=========================================\nIRQ Status:\n");
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		t_diff = t_cur - per_cpu(save_irq_count_time, cpu);
		pr_info
		("Cur irq cnts record %llu ns.(last at %llu,diff:+%llu ns)\n",
		t_cur,
		per_cpu(save_irq_count_time, cpu),
		usec_high(t_diff));
		pr_info("CPU%d state:%s\n",
			cpu, cpu_online(cpu) ? "online" : "offline");
		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			count = kstat_irqs_cpu(irq, cpu);
			if (count != 0) {
				pr_info(" IRQ[%3d:%14s]=%8d,",
				irq,
				isr_name(irq),
				count);
				pr_info(" (+%d times in %lld us)\n",
				count - per_cpu(irq_count_mon, cpu).irqs[irq],
				usec_high(t_diff));
			}
		}
	}
#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		pr_info("Local IRQ on CPU#%d:\n", cpu);
		for (irq = 0; irq < NR_IPI; irq++) {
			count = __get_irq_stat(cpu, ipi_irqs[irq]);
			if (count != 0) {
				pr_info(" IRQ[%2d:  IPI] = %8d,",
				irq, count);
				pr_info(" (+%d times in %lld us)\n",
				count - per_cpu(ipi_count_mon, cpu).ipis[irq],
				usec_high(t_diff));
			}
		}
	}
#endif
	spin_unlock_irqrestore(&mt_irq_count_lock, flags);
}

void mt_show_timer_info(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		pr_info("[TimerISR#%d] last timer ISR",
			cpu);
		pr_info(" start:%llu ns, end:%llu ns\n",
			per_cpu(local_timer_ts, cpu),
			per_cpu(local_timer_te, cpu));
	}
}

/* for MTK fiq debug log mechanism*/
static void mt_aee_show_current_irq_counts(void)
{

	int irq, cpu, count;
	unsigned long long t_cur, t_diff;

	t_cur = sched_clock();
	/* spin_lock_irqsave(&mt_irq_count_lock, flags); */

	snprintf(wdt_sched_mon_log_buf,
		sizeof(wdt_sched_mon_log_buf), "\nIRQ Status\n");
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
	memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		t_diff = t_cur - per_cpu(save_irq_count_time, cpu);
		snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
			 "Dur:%lld us,(now:%lld,last:%lld)\n",
			 usec_high(t_diff), usec_high(t_cur),
			 usec_high(per_cpu(save_irq_count_time, cpu)));
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
		memset(wdt_sched_mon_log_buf, 0,
			sizeof(wdt_sched_mon_log_buf));

		snprintf(wdt_sched_mon_log_buf,
			sizeof(wdt_sched_mon_log_buf), "CPU%d state:%s\n",
			cpu, cpu_online(cpu) ? "online" : "offline");
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
		memset(wdt_sched_mon_log_buf, 0, sizeof(wdt_sched_mon_log_buf));

		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			count = kstat_irqs_cpu(irq, cpu);
			if (count != 0) {
				snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				" %d:%s +%d(%d)\n", irq, isr_name(irq),
				count - per_cpu(irq_count_mon, cpu).irqs[irq],
				count);
#ifdef CONFIG_MTK_RAM_CONSOLE
				aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
				memset(wdt_sched_mon_log_buf, 0,
					sizeof(wdt_sched_mon_log_buf));
			}
		}
	}
#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		snprintf(wdt_sched_mon_log_buf,
			sizeof(wdt_sched_mon_log_buf), "CPU#%d:\n", cpu);
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
		memset(wdt_sched_mon_log_buf, 0,
			sizeof(wdt_sched_mon_log_buf));
		for (irq = 0; irq < NR_IPI; irq++) {
			count = __get_irq_stat(cpu, ipi_irqs[irq]);
			if (count != 0) {
				snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				" %d:IPI +%d(%d)\n", irq,
				count - per_cpu(ipi_count_mon, cpu).ipis[irq],
				count);
				#if defined(CONFIG_MTK_AEE_FEATURE)
				aee_sram_fiq_log(wdt_sched_mon_log_buf);
				#endif
				memset(wdt_sched_mon_log_buf, 0,
					sizeof(wdt_sched_mon_log_buf));
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
		snprintf(wdt_sched_mon_log_buf,
			sizeof(wdt_sched_mon_log_buf),
			"[TimerISR#%d]last s:%llu e:%llu ns\n",
			cpu, per_cpu(local_timer_ts, cpu),
			per_cpu(local_timer_te, cpu));
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
		memset(wdt_sched_mon_log_buf, 0,
			sizeof(wdt_sched_mon_log_buf));
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
		pr_info("==CPU[%d]==\n", cpu);
		if (b_isr->cur_event == 0) {
			pr_info("[ISR]last_irq:%d,dur:%llu",
				(int)b_isr->last_event,
				b_isr->last_te - b_isr->last_ts);
			pr_info(" ns(s:%llu, e:%llu)\n\n",
				b_isr->last_ts, b_isr->last_te);
		} else {
			pr_info("[InISR]Current irq:%d,",
				(int)b_isr->cur_event);
			pr_info(" Start:%llu(elapsed: %llu ns),",
				b_isr->cur_ts, sched_clock() - b_isr->cur_ts);
			pr_info(" last irq#:%d,last_start:%llu,",
				(int)b_isr->last_event,
				b_isr->last_ts);
			pr_info(" last_end:%llu\n\n",
				b_isr->last_te);
		}

		if (b_sq->cur_event == 0) {
			pr_info("[Softirq]last#:%d,dur:%llu",
				(int)b_sq->last_event,
				b_sq->last_te - b_sq->last_ts);
			pr_info(" ns(s:%llu,e:%llu)\n\n",
				b_sq->last_ts, b_sq->last_te);
		} else {
			pr_info("[InSoftirq]Current sirq#:%d,",
				(int)b_sq->cur_event);
			pr_info(" Start:%llu(elapsed:%llu ns),",
				b_sq->cur_ts, sched_clock() - b_sq->cur_ts);
			pr_info(" last sirq#:%d(dur:%llu ns),",
				(int)b_sq->last_event,
				b_sq->last_te - b_sq->last_ts);
			pr_info(" last_start:%llu,last_end:%llu\n\n",
				b_sq->last_ts, b_sq->last_te);
		}

		if (b_tk->cur_event == 0) {
			pr_info("[Tasklet]Occurs %d times in last SIRQ dur\n",
				(int)b_tk->last_count);
			pr_info(" last fn:%pS, dur:%llu ns(s:%llu,e:%llu)\n\n",
				(void *)b_tk->last_event,
				b_tk->last_te - b_tk->last_ts,
				b_tk->last_ts, b_tk->last_te);
		} else {
			pr_info("[In Tasklet]\n Occurs:cur:%d,",
				(int)b_tk->cur_count);
			pr_info(" last:%d\n Cur:%pS,",
				(int)b_tk->last_count,
				(void *)b_tk->cur_event);
			pr_info(" Start:%llu(elapsed: %llu ns),",
				b_tk->cur_ts, sched_clock() - b_tk->cur_ts);
			pr_info(" last#:%pS(dur:%llu ns),",
				(void *)b_tk->last_event,
				b_tk->last_te - b_tk->last_ts);
			pr_info(" last_start:%llu, last_end:%llu\n\n",
				b_tk->last_ts, b_tk->last_te);
		}

		if (b_hrt->cur_event == 0) {
			pr_info("[HRTimer]\n Occurs");
			pr_info(" %d times in lasat ISR duration\n",
				(int)b_hrt->last_count);
			pr_info(" last fn:%pS,dur:%llu ns(s:%llu, e:%llu)\n\n",
				(void *)b_hrt->last_event,
				b_hrt->last_te - b_hrt->last_ts,
				b_hrt->last_ts, b_hrt->last_te);
		} else {
			pr_info("[In HRTimer]\n Occurs: cur:%d,",
				(int)b_tk->cur_count);
			pr_info(" last:%d\n Current:%pS, ",
				(int)b_tk->last_count,
				(void *)b_hrt->cur_event);
			pr_info(" Start:%llu(elapsed: %llu ns),",
				b_hrt->cur_ts, sched_clock() - b_hrt->cur_ts);
			pr_info(" last#:%pS(dur:%llu ns), ",
				(void *)b_hrt->last_event,
				b_hrt->last_te - b_hrt->last_ts);
			pr_info(" last_start:%llu, last_end:%llu\n\n",
				b_hrt->last_ts, b_hrt->last_te);
		}

		if (b_sft->cur_event == 0) {
			pr_info("[SoftTimer]\n Occurs");
			pr_info(" %d times in last SoftIRQ duration\n",
				(int)b_sft->last_count);

			pr_info(" last fn:%pS,dur:%llu ns(s:%llu, e:%llu)\n\n",
				(void *)b_sft->last_event,
				b_sft->last_te - b_sft->last_ts,
				b_sft->last_ts, b_sft->last_te);
		} else {
			pr_info("[In SoftTimer]\n Occurs: cur:%d,",
				(int)b_sft->cur_count);
			pr_info(" last:%d\n Current:%pS,",
				(int)b_sft->last_count,
				(void *)b_sft->cur_event);
			pr_info(" Start:%llu(elapsed: %llu ns),",
				b_sft->cur_ts, sched_clock() - b_sft->cur_ts);
			pr_info(" last#:%pS(dur:%llu ns),",
				(void *)b_sft->last_event,
				b_sft->last_te - b_sft->last_ts);
			pr_info(" last_start:%llu, last_end:%llu\n\n",
				b_sft->last_ts, b_sft->last_te);
		}

/****  Dump Stop Events ****/
		if (e_irq->cur_ts == 0) {
			pr_info("[IRQ disable] last duration:");
			pr_info(" %llu ns (s: %llu, e: %llu)\n\n",
				e_irq->last_te - e_irq->last_ts,
				e_irq->last_ts, e_irq->last_te);
		} else {
			pr_info("[IRQ disable] cur_ts:%llu(elapsed:%llu ns),",
				e_irq->cur_ts, sched_clock() - e_irq->cur_ts);
			pr_info(" last duration:%llu ns(s: %llu, e: %llu)\n\n",
				e_irq->last_te - e_irq->last_ts, e_irq->last_ts,
				e_irq->last_te);
		}
		if (e_pmpt->cur_ts == 0) {
			pr_info("[Preempt disable] last duration:");
			pr_info(" %llu ns(s: %llu, e: %llu)\n\n",
				e_pmpt->last_te - e_pmpt->last_ts,
				e_pmpt->last_ts, e_pmpt->last_te);
		} else {
			pr_info("[Preempt disable] cur_ts:%llu(elapsed:%llu ns)",
				e_pmpt->cur_ts,
				sched_clock() - e_pmpt->cur_ts);
			pr_info("last duration:%llu ns(s: %llu, e: %llu)\n\n",
				e_pmpt->last_te - e_pmpt->last_ts,
				e_pmpt->last_ts, e_pmpt->last_te);
		}
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

		snprintf(wdt_sched_mon_log_buf, sizeof(wdt_sched_mon_log_buf),
			 "CPU%d\n", cpu);
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
		memset(wdt_sched_mon_log_buf, 0,
			sizeof(wdt_sched_mon_log_buf));

		if (b_isr->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[ISR]last#%d,dur:%lld s:%lld\n\n",
				(int)b_isr->last_event,
				usec_high(b_isr->last_te - b_isr->last_ts),
				usec_high(b_isr->last_ts));
#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		} else {
			ssize_t count = 0;

			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[In ISR]Current irq#:%d, Start:%lld (elapsed: %lld),",
				(int)b_isr->cur_event,
				usec_high(b_isr->cur_ts),
				usec_high(sched_clock() - b_isr->cur_ts));
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" last irq#:%d, s:%lld, e:%lld\n\n",
				(int)b_isr->last_event,
				usec_high(b_isr->last_ts),
				usec_high(b_isr->last_te));

#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		}

		if (b_sq->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[Softirq]last#%d,dur:%lld s:%lld\n\n",
				(int)b_sq->last_event,
				usec_high(b_sq->last_te - b_sq->last_ts),
				usec_high(b_sq->last_ts));
#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		} else {
			ssize_t count = 0;

			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[In Softirq]Current softirq#:%d,",
				(int)b_sq->cur_event);
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" Start:%lld(elapsed: %lld),",
				usec_high(b_sq->cur_ts),
				usec_high(sched_clock() - b_sq->cur_ts));

#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				" last softirq#:%d(dur:%lld),",
				(int)b_sq->last_event,
				usec_high(b_sq->last_te - b_sq->last_ts));
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" s:%lld, e:%lld\n\n",
				usec_high(b_sq->last_ts),
				usec_high(b_sq->last_te));

#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		}

		if (b_tk->cur_event == 0) {
			ssize_t count = 0;

			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[Tasklet]%d/SoftIRQ\n %pS",
				(int)b_tk->last_count,
				(void *)b_tk->last_event);
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" dur:%lld s:%lld\n\n",
				usec_high(b_tk->last_te - b_tk->last_ts),
				usec_high(b_tk->last_ts));

#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		} else {
			ssize_t count = 0;

			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[In Tasklet]\n Occurs: cur:%d, last:%d\n",
				(int)b_tk->cur_count,
				(int)b_tk->last_count);
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" Current:%pS, Start:%lld(elapsed: %lld),",
				(void *)b_tk->cur_event,
				usec_high(b_tk->cur_ts),
				usec_high(sched_clock() - b_tk->cur_ts));

#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				" last#:%pS(dur:%lld),",
				(void *)b_tk->last_event,
				usec_high(b_tk->last_te - b_tk->last_ts));
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" last_start:%lld, last_end:%lld\n\n",
				usec_high(b_tk->last_ts),
				usec_high(b_tk->last_te));

#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		}

		if (b_hrt->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[HRTimer]%d/ISR\n %pS dur:%lld s:%lld\n\n",
				(int)b_hrt->last_count,
				(void *)b_hrt->last_event,
				usec_high(b_hrt->last_te - b_hrt->last_ts),
				usec_high(b_hrt->last_ts));
#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		} else {
			ssize_t count = 0;

			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[In HRTimer]\n Occurs: cur:%d, last:%d\n",
				(int)b_tk->cur_count,
				(int)b_tk->last_count);
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" Current:%pS, Start:%lld(elapsed: %lld),",
				(void *)b_hrt->cur_event,
				usec_high(b_hrt->cur_ts),
				usec_high(sched_clock() - b_hrt->cur_ts));

#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				" last#:%pS(dur:%lld),",
				(void *)b_hrt->last_event,
				usec_high(b_hrt->last_te - b_hrt->last_ts));
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" last_start:%lld, last_end:%lld\n\n",
				usec_high(b_hrt->last_ts),
				usec_high(b_hrt->last_te));



#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		}

		if (b_sft->cur_event == 0) {
			snprintf(wdt_sched_mon_log_buf,
			sizeof(wdt_sched_mon_log_buf),
			"[SoftTimer]%d/SoftIRQ\n %pS dur:%lld s:%lld\n\n",
			(int)b_sft->last_count,
			(void *)b_sft->last_event,
			usec_high(b_sft->last_te - b_sft->last_ts),
			usec_high(b_sft->last_ts));
#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		} else {
			ssize_t count = 0;

			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				"[In SoftTimer]\n Occurs: cur:%d, last:%d\n",
				(int)b_sft->cur_count,
				(int)b_sft->last_count);
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" Current:%pS, Start:%lld(elapsed: %lld),",
				(void *)b_sft->cur_event,
				usec_high(b_sft->cur_ts),
				usec_high(sched_clock() - b_sft->cur_ts));


#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
			count = snprintf(wdt_sched_mon_log_buf,
				sizeof(wdt_sched_mon_log_buf),
				" last#:%pS(dur:%lld), last_start:%lld,",
				(void *)b_sft->last_event,
				usec_high(b_sft->last_te - b_sft->last_ts),
				usec_high(b_sft->last_ts));
			snprintf(wdt_sched_mon_log_buf + count,
				sizeof(wdt_sched_mon_log_buf) - count,
				" last_end:%lld\n\n",
				usec_high(b_sft->last_te));

#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_sram_fiq_log(wdt_sched_mon_log_buf);
#endif
			memset(wdt_sched_mon_log_buf, 0,
				sizeof(wdt_sched_mon_log_buf));
		}
		/****  Dump Stop Events ****/
		/*
		 * e_irq->cur_ts == 0?
		 aee_wdt_printf("[IRQ disable] last
		 duration:%llu ns (s: %llu, e: %llu)\n\n",
		 e_irq->last_te - e_irq->last_ts,
		 e_irq->last_ts, e_irq->last_te):
		 aee_wdt_printf("[IRQ disable] cur_ts:%llu(elapsed:%llu ns), "
		 "last duration:%llu ns(s: %llu, e: %llu)\n\n",
		 e_irq->cur_ts, sched_clock() - e_irq->cur_ts,
		 e_irq->last_te - e_irq->last_ts,"
		 " e_irq->last_ts, e_irq->last_te);

		 e_pmpt->cur_ts == 0?
		 aee_wdt_printf("[Preempt disable] last
		 duration:%llu ns(s: %llu, e: %llu)\n\n",
		 e_pmpt->last_te - e_pmpt->last_ts,
		 e_pmpt->last_ts, e_pmpt->last_te):
		 aee_wdt_printf("[Preempt disable]
		 cur_ts:%llu(elapsed:%llu ns), "
		 "last duration:%llu ns(s: %llu, e: %llu)\n\n",
		 e_pmpt->cur_ts, sched_clock() - e_pmpt->cur_ts,
		 e_pmpt->last_te - e_pmpt->last_ts,"
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
