#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>
#include "prof_ctl.h"
#include <linux/module.h>
#include <linux/pid.h>

#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/interrupt.h>

#include <linux/mt_sched_mon.h>

#include <linux/aee.h>
#include <linux/stacktrace.h>

#ifdef CONFIG_MT_SCHED_MONITOR
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

void mt_show_last_irq_counts(void);
void mt_show_current_irq_counts(void);
void mt_show_timer_info(void);
static void mt_aee_show_current_irq_counts(void);
static void mt_aee_show_timer_info(void);
void mt_save_irq_counts(void);
extern void aee_wdt_printf(const char *fmt, ...);
void mt_dump_sched_traces(void)
{
	struct sched_block_event *b_isr, *b_sq, *b_tk, *b_hrt, *b_sft;
	struct sched_stop_event *e_irq, *e_pmpt;
	int cpu;
	for_each_possible_cpu(cpu) {

		b_isr = &per_cpu(ISR_mon, cpu);
		b_sq = &per_cpu(SoftIRQ_mon, cpu);
		b_tk = &per_cpu(tasklet_mon, cpu);
		b_hrt = &per_cpu(hrt_mon, cpu);
		b_sft = &per_cpu(sft_mon, cpu);

		e_pmpt = &per_cpu(Preempt_disable_mon, cpu);
		e_irq = &per_cpu(IRQ_disable_mon, cpu);
		pr_err("==CPU[%d]==\n", cpu);
		b_isr->cur_event == 0 ?
		    pr_err("[ISR]last_irq:%d, dur:%llu ns (s:%llu, e:%llu)\n\n",
			   (int)b_isr->last_event, b_isr->last_te - b_isr->last_ts, b_isr->last_ts,
			   b_isr->
			   last_te) :
		    pr_err
		    ("[In ISR]Current irq:%d, Start:%llu (elapsed: %llu ns), last irq#:%d, last_start:%llu, last_end:%llu\n\n",
		     (int)b_isr->cur_event, b_isr->cur_ts, sched_clock() - b_isr->cur_ts,
		     (int)b_isr->last_event, b_isr->last_ts, b_isr->last_te);

		b_sq->cur_event == 0 ?
		    pr_err("[Softirq]last#:%d, dur:%llu ns (s:%llu, e:%llu)\n\n",
			   (int)b_sq->last_event, b_sq->last_te - b_sq->last_ts, b_sq->last_ts,
			   b_sq->
			   last_te) :
		    pr_err
		    ("[In Softirq]Current softirq#:%d, Start:%llu(elapsed: %llu ns), last softirq#:%d(dur:%llu ns), last_start:%llu, last_end:%llu\n\n",
		     (int)b_sq->cur_event, b_sq->cur_ts, sched_clock() - b_sq->cur_ts,
		     (int)b_sq->last_event, b_sq->last_te - b_sq->last_ts, b_sq->last_ts,
		     b_sq->last_te);


		b_tk->cur_event == 0 ?
		    pr_err
		    ("[Tasklet]\n Occurs %d times in last SoftIRQ duration\n last fn:%pS, dur:%llu ns (s:%llu, e:%llu)\n\n",
		     (int)b_tk->last_count, (void *)b_tk->last_event, b_tk->last_te - b_tk->last_ts,
		     b_tk->last_ts,
		     b_tk->
		     last_te) :
		    pr_err
		    ("[In Tasklet]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%llu(elapsed: %llu ns), last#:%pS(dur:%llu ns), last_start:%llu, last_end:%llu\n\n",
		     (int)b_tk->cur_count, (int)b_tk->last_count, (void *)b_tk->cur_event,
		     b_tk->cur_ts, sched_clock() - b_tk->cur_ts, (void *)b_tk->last_event,
		     b_tk->last_te - b_tk->last_ts, b_tk->last_ts, b_tk->last_te);

		b_hrt->cur_event == 0 ?
		    pr_err
		    ("[HRTimer]\n Occurs %d times in last ISR duration\n last fn:%pS, dur:%llu ns (s:%llu, e:%llu)\n\n",
		     (int)b_hrt->last_count, (void *)b_hrt->last_event,
		     b_hrt->last_te - b_hrt->last_ts, b_hrt->last_ts,
		     b_hrt->
		     last_te) :
		    pr_err
		    ("[In HRTimer]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%llu(elapsed: %llu ns), last#:%pS(dur:%llu ns), last_start:%llu, last_end:%llu\n\n",
		     (int)b_tk->cur_count, (int)b_tk->last_count, (void *)b_hrt->cur_event,
		     b_hrt->cur_ts, sched_clock() - b_hrt->cur_ts, (void *)b_hrt->last_event,
		     b_hrt->last_te - b_hrt->last_ts, b_hrt->last_ts, b_hrt->last_te);

		b_sft->cur_event == 0 ?
		    pr_err
		    ("[SoftTimer]\n Occurs %d times in last SoftIRQ duration\n last fn:%pS, dur:%llu ns (s:%llu, e:%llu)\n\n",
		     (int)b_sft->last_count, (void *)b_sft->last_event,
		     b_sft->last_te - b_sft->last_ts, b_sft->last_ts,
		     b_sft->
		     last_te) :
		    pr_err
		    ("[In SoftTimer]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%llu(elapsed: %llu ns), last#:%pS(dur:%llu ns), last_start:%llu, last_end:%llu\n\n",
		     (int)b_sft->cur_count, (int)b_sft->last_count, (void *)b_sft->cur_event,
		     b_sft->cur_ts, sched_clock() - b_sft->cur_ts, (void *)b_sft->last_event,
		     b_sft->last_te - b_sft->last_ts, b_sft->last_ts, b_sft->last_te);

/****  Dump Stop Events ****/
		e_irq->cur_ts == 0 ?
		    pr_err("[IRQ disable] last duration:%llu ns (s: %llu, e: %llu)\n\n",
			   e_irq->last_te - e_irq->last_ts, e_irq->last_ts, e_irq->last_te) :
		    pr_err
		    ("[IRQ disable] cur_ts:%llu(elapsed:%llu ns), last duration:%llu ns(s: %llu, e: %llu)\n\n",
		     e_irq->cur_ts, sched_clock() - e_irq->cur_ts, e_irq->last_te - e_irq->last_ts,
		     e_irq->last_ts, e_irq->last_te);


		e_pmpt->cur_ts == 0 ?
		    pr_err("[Preempt disable] last duration:%llu ns(s: %llu, e: %llu)\n\n",
			   e_pmpt->last_te - e_pmpt->last_ts, e_pmpt->last_ts, e_pmpt->last_te) :
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

		b_isr = &per_cpu(ISR_mon, cpu);
		b_sq = &per_cpu(SoftIRQ_mon, cpu);
		b_tk = &per_cpu(tasklet_mon, cpu);
		b_hrt = &per_cpu(hrt_mon, cpu);
		b_sft = &per_cpu(sft_mon, cpu);

		e_pmpt = &per_cpu(Preempt_disable_mon, cpu);
		e_irq = &per_cpu(IRQ_disable_mon, cpu);
		aee_wdt_printf("CPU%d\n", cpu);
		b_isr->cur_event == 0 ?
		    aee_wdt_printf("[ISR]last#%d,dur:%lld s:%lld\n\n",
				   (int)b_isr->last_event,
				   usec_high(b_isr->last_te - b_isr->last_ts),
				   usec_high(b_isr->
					     last_ts)) :
		    aee_wdt_printf
		    ("[In ISR]Current irq#:%d, Start:%lld (elapsed: %lld), last irq#:%d, s:%lld, e:%lld\n\n",
		     (int)b_isr->cur_event, usec_high(b_isr->cur_ts),
		     usec_high(sched_clock() - b_isr->cur_ts), (int)b_isr->last_event,
		     usec_high(b_isr->last_ts), usec_high(b_isr->last_te));

		b_sq->cur_event == 0 ?
		    aee_wdt_printf("[Softirq]last#%d,dur:%lld s:%lld\n\n",
				   (int)b_sq->last_event, usec_high(b_sq->last_te - b_sq->last_ts),
				   usec_high(b_sq->
					     last_ts)) :
		    aee_wdt_printf
		    ("[In Softirq]Current softirq#:%d, Start:%lld(elapsed: %lld), last softirq#:%d(dur:%lld), s:%lld, e:%lld\n\n",
		     (int)b_sq->cur_event, usec_high(b_sq->cur_ts),
		     usec_high(sched_clock() - b_sq->cur_ts), (int)b_sq->last_event,
		     usec_high(b_sq->last_te - b_sq->last_ts), usec_high(b_sq->last_ts),
		     usec_high(b_sq->last_te));


		b_tk->cur_event == 0 ?
		    aee_wdt_printf("[Tasklet]%d/SoftIRQ\n %pS dur:%lld s:%lld\n\n",
				   (int)b_tk->last_count, (void *)b_tk->last_event,
				   usec_high(b_tk->last_te - b_tk->last_ts),
				   usec_high(b_tk->
					     last_ts)) :
		    aee_wdt_printf
		    ("[In Tasklet]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%lld(elapsed: %lld), last#:%pS(dur:%lld), last_start:%lld, last_end:%lld\n\n",
		     (int)b_tk->cur_count, (int)b_tk->last_count, (void *)b_tk->cur_event,
		     usec_high(b_tk->cur_ts), usec_high(sched_clock() - b_tk->cur_ts),
		     (void *)b_tk->last_event, usec_high(b_tk->last_te - b_tk->last_ts),
		     usec_high(b_tk->last_ts), usec_high(b_tk->last_te));

		b_hrt->cur_event == 0 ?
		    aee_wdt_printf("[HRTimer]%d/ISR\n %pS dur:%lld s:%lld\n\n",
				   (int)b_hrt->last_count, (void *)b_hrt->last_event,
				   usec_high(b_hrt->last_te - b_hrt->last_ts),
				   usec_high(b_hrt->
					     last_ts)) :
		    aee_wdt_printf
		    ("[In HRTimer]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%lld(elapsed: %lld), last#:%pS(dur:%lld), last_start:%lld, last_end:%lld\n\n",
		     (int)b_tk->cur_count, (int)b_tk->last_count, (void *)b_hrt->cur_event,
		     usec_high(b_hrt->cur_ts), usec_high(sched_clock() - b_hrt->cur_ts),
		     (void *)b_hrt->last_event, usec_high(b_hrt->last_te - b_hrt->last_ts),
		     usec_high(b_hrt->last_ts), usec_high(b_hrt->last_te));

		b_sft->cur_event == 0 ?
		    aee_wdt_printf("[SoftTimer]%d/SoftIRQ\n %pS dur:%lld s:%lld\n\n",
				   (int)b_sft->last_count, (void *)b_sft->last_event,
				   usec_high(b_sft->last_te - b_sft->last_ts),
				   usec_high(b_sft->
					     last_ts)) :
		    aee_wdt_printf
		    ("[In SoftTimer]\n Occurs: cur:%d, last:%d\n Current:%pS, Start:%lld(elapsed: %lld), last#:%pS(dur:%lld), last_start:%lld, last_end:%lld\n\n",
		     (int)b_sft->cur_count, (int)b_sft->last_count, (void *)b_sft->cur_event,
		     usec_high(b_sft->cur_ts), usec_high(sched_clock() - b_sft->cur_ts),
		     (void *)b_sft->last_event, usec_high(b_sft->last_te - b_sft->last_ts),
		     usec_high(b_sft->last_ts), usec_high(b_sft->last_te));

/****  Dump Stop Events ****/
		/*
		   e_irq->cur_ts == 0?
		   aee_wdt_printf("[IRQ disable] last duration:%llu ns (s: %llu, e: %llu)\n\n",
		   e_irq->last_te - e_irq->last_ts, e_irq->last_ts, e_irq->last_te):
		   aee_wdt_printf("[IRQ disable] cur_ts:%llu(elapsed:%llu ns), last duration:%llu ns(s: %llu, e: %llu)\n\n",
		   e_irq->cur_ts, sched_clock() - e_irq->cur_ts, e_irq->last_te - e_irq->last_ts, e_irq->last_ts, e_irq->last_te);

		   e_pmpt->cur_ts == 0?
		   aee_wdt_printf("[Preempt disable] last duration:%llu ns(s: %llu, e: %llu)\n\n",
		   e_pmpt->last_te - e_pmpt->last_ts, e_pmpt->last_ts, e_pmpt->last_te):
		   aee_wdt_printf("[Preempt disable] cur_ts:%llu(elapsed:%llu ns), last duration:%llu ns(s: %llu, e: %llu)\n\n",
		   e_pmpt->cur_ts, sched_clock() - e_pmpt->cur_ts, e_pmpt->last_te - e_pmpt->last_ts, e_pmpt->last_ts, e_pmpt->last_te);
		 */
	}
	mt_aee_show_current_irq_counts();
	mt_aee_show_timer_info();
}

#include <linux/irqnr.h>
#include <linux/kernel_stat.h>
#include <asm/hardirq.h>

extern int mt_irq_count[NR_CPUS][MAX_NR_IRQS];
#ifdef CONFIG_SMP
extern int mt_local_irq_count[NR_CPUS][NR_IPI];
#endif
extern unsigned long long mt_save_irq_count_time;
extern spinlock_t mt_irq_count_lock;

void mt_show_last_irq_counts(void)
{
	int irq, cpu;
	unsigned long flags;
	spin_lock_irqsave(&mt_irq_count_lock, flags);
	pr_err("Last irq counts record at [%llu] ns\n", mt_save_irq_count_time);
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		pr_err("CPU#%d:\n", cpu);
		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			if (mt_irq_count[cpu][irq] != 0)
				pr_err("[%3d] = %8d\n", irq, mt_irq_count[cpu][irq]);
		}
	}

#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		for (irq = 0; irq < NR_IPI; irq++) {
			if (mt_local_irq_count[cpu][irq] != 0)
				pr_err("(CPU#%d)IPI[%3d] = %8d\n", cpu, irq,
				       mt_local_irq_count[cpu][irq]);
		}
	}
#endif
	spin_unlock_irqrestore(&mt_irq_count_lock, flags);
}

void mt_show_current_irq_counts(void)
{
	int irq, cpu, count;
	unsigned long flags;
	unsigned long long t_cur, t_diff;
	t_cur = sched_clock();
	spin_lock_irqsave(&mt_irq_count_lock, flags);

	t_diff = t_cur - mt_save_irq_count_time;
	pr_err("=========================================\nIRQ Status:\n");
	pr_err("Current irq counts record at [%llu] ns.(last at %llu, diff:+%llu ns)\n", t_cur,
	       mt_save_irq_count_time, t_diff);
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		pr_err(" --CPU%d--\n", cpu);
		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			count = kstat_irqs_cpu(irq, cpu);
			if (count != 0)
				pr_err(" IRQ[%3d:%14s] = %8d, (+%d times in %lld us)\n", irq,
				       isr_name(irq), count, count - mt_irq_count[cpu][irq],
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
				       count - mt_local_irq_count[cpu][irq], usec_high(t_diff));
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

	t_diff = t_cur - mt_save_irq_count_time;
	aee_wdt_printf("\nIRQ Status\n");
	aee_wdt_printf("Dur:%lld us,(now:%lld,last:%lld)\n", usec_high(t_diff), usec_high(t_cur),
		       mt_save_irq_count_time);
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		aee_wdt_printf(" CPU%d\n", cpu);
		for (irq = 0; irq < nr_irqs && irq < MAX_NR_IRQS; irq++) {
			count = kstat_irqs_cpu(irq, cpu);
			if (count != 0)
				aee_wdt_printf(" %d:%s +%d(%d)\n", irq, isr_name(irq),
					       count - mt_irq_count[cpu][irq], count);
		}
	}
#ifdef CONFIG_SMP
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		aee_wdt_printf("CPU#%d:\n", cpu);
		for (irq = 0; irq < NR_IPI; irq++) {
			count = __get_irq_stat(cpu, ipi_irqs[irq]);
			if (count != 0)
				aee_wdt_printf(" %d:IPI +%d(%d)\n", irq,
					       count - mt_local_irq_count[cpu][irq], count);
		}
	}
#endif
	/* spin_unlock_irqrestore(&mt_irq_count_lock, flags); */
}

static void mt_aee_show_timer_info(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		aee_wdt_printf("[TimerISR#%d]last s:%llu e:%llu ns\n", cpu,
			       per_cpu(local_timer_ts, cpu), per_cpu(local_timer_te, cpu));
	}
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
