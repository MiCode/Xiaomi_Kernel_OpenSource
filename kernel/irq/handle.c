/*
 * linux/kernel/irq/handle.c
 *
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner, Russell King
 *
 * This file contains the core interrupt handling code.
 *
 * Detailed information is available in Documentation/DocBook/genericirq
 *
 */

#include <linux/irq.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/slab.h>

#include <trace/events/irq.h>

#include "internals.h"
#ifdef CONFIG_MT_RT_THROTTLE_MON
#include "mt_sched_mon.h"
#endif
#ifdef CONFIG_MTPROF_CPUTIME
#include "mt_cputime.h"
/*  cputime monitor en/disable value */
#ifdef CONFIG_MT_ENG_BUILD
/* max debug thread count, if reach the level, stop store new thread informaiton. */
#define MAX_THREAD_COUNT (6000)
#else
#define MAX_THREAD_COUNT (3000)
#endif
#endif

/**
 * handle_bad_irq - handle spurious and unhandled irqs
 * @irq:       the interrupt number
 * @desc:      description of the interrupt
 *
 * Handles spurious and unhandled IRQ's. It also prints a debugmessage.
 */
void handle_bad_irq(unsigned int irq, struct irq_desc *desc)
{
	print_irq_desc(irq, desc);
	kstat_incr_irqs_this_cpu(irq, desc);
	ack_bad_irq(irq);
}

/*
 * Special, empty irq handler:
 */
irqreturn_t no_action(int cpl, void *dev_id)
{
	return IRQ_NONE;
}
EXPORT_SYMBOL_GPL(no_action);

static void warn_no_thread(unsigned int irq, struct irqaction *action)
{
	if (test_and_set_bit(IRQTF_WARNED, &action->thread_flags))
		return;

	printk(KERN_WARNING "IRQ %d device %s returned IRQ_WAKE_THREAD "
	       "but no thread function available.", irq, action->name);
}

void __irq_wake_thread(struct irq_desc *desc, struct irqaction *action)
{
	/*
	 * In case the thread crashed and was killed we just pretend that
	 * we handled the interrupt. The hardirq handler has disabled the
	 * device interrupt, so no irq storm is lurking.
	 */
	if (action->thread->flags & PF_EXITING)
		return;

	/*
	 * Wake up the handler thread for this action. If the
	 * RUNTHREAD bit is already set, nothing to do.
	 */
	if (test_and_set_bit(IRQTF_RUNTHREAD, &action->thread_flags))
		return;

	/*
	 * It's safe to OR the mask lockless here. We have only two
	 * places which write to threads_oneshot: This code and the
	 * irq thread.
	 *
	 * This code is the hard irq context and can never run on two
	 * cpus in parallel. If it ever does we have more serious
	 * problems than this bitmask.
	 *
	 * The irq threads of this irq which clear their "running" bit
	 * in threads_oneshot are serialized via desc->lock against
	 * each other and they are serialized against this code by
	 * IRQS_INPROGRESS.
	 *
	 * Hard irq handler:
	 *
	 *	spin_lock(desc->lock);
	 *	desc->state |= IRQS_INPROGRESS;
	 *	spin_unlock(desc->lock);
	 *	set_bit(IRQTF_RUNTHREAD, &action->thread_flags);
	 *	desc->threads_oneshot |= mask;
	 *	spin_lock(desc->lock);
	 *	desc->state &= ~IRQS_INPROGRESS;
	 *	spin_unlock(desc->lock);
	 *
	 * irq thread:
	 *
	 * again:
	 *	spin_lock(desc->lock);
	 *	if (desc->state & IRQS_INPROGRESS) {
	 *		spin_unlock(desc->lock);
	 *		while(desc->state & IRQS_INPROGRESS)
	 *			cpu_relax();
	 *		goto again;
	 *	}
	 *	if (!test_bit(IRQTF_RUNTHREAD, &action->thread_flags))
	 *		desc->threads_oneshot &= ~mask;
	 *	spin_unlock(desc->lock);
	 *
	 * So either the thread waits for us to clear IRQS_INPROGRESS
	 * or we are waiting in the flow handler for desc->lock to be
	 * released before we reach this point. The thread also checks
	 * IRQTF_RUNTHREAD under desc->lock. If set it leaves
	 * threads_oneshot untouched and runs the thread another time.
	 */
	desc->threads_oneshot |= action->thread_mask;

	/*
	 * We increment the threads_active counter in case we wake up
	 * the irq thread. The irq thread decrements the counter when
	 * it returns from the handler or in the exit path and wakes
	 * up waiters which are stuck in synchronize_irq() when the
	 * active count becomes zero. synchronize_irq() is serialized
	 * against this code (hard irq handler) via IRQS_INPROGRESS
	 * like the finalize_oneshot() code. See comment above.
	 */
	atomic_inc(&desc->threads_active);

	wake_up_process(action->thread);
}

#if defined(CONFIG_MTPROF_CPUTIME) || defined(CONFIG_MT_RT_THROTTLE_MON)
static void save_isr_info(unsigned int irq, struct irqaction *action,
			  unsigned long long start, unsigned long long end)
{
	unsigned long long dur = end - start;
#ifdef CONFIG_MTPROF_CPUTIME
	int isr_find = 0;
	struct mtk_isr_info *mtk_isr_point = current->se.mtk_isr;
	struct mtk_isr_info *mtk_isr_current = mtk_isr_point;
	char *isr_name = NULL;

	if (unlikely(mtsched_is_enabled())) {
		current->se.mtk_isr_time += dur;
		while ((current->se.mtk_isr != NULL) && (mtk_isr_point != NULL)) {
			if (mtk_isr_point->isr_num == irq) {
				mtk_isr_point->isr_time += dur;
				mtk_isr_point->isr_count++;
				isr_find = 1;
				break;
			}
			mtk_isr_current = mtk_isr_point;
			mtk_isr_point = mtk_isr_point->next;
		}

		if ((isr_find == 0) && (mtproc_counts() < MAX_THREAD_COUNT)) {
			mtk_isr_point =  kmalloc(sizeof(struct mtk_isr_info), GFP_ATOMIC);
			if (mtk_isr_point == NULL) {
				pr_debug("cant' alloc mtk_isr_info mem!\n");
			} else {
				mtk_isr_point->isr_num = irq;
				mtk_isr_point->isr_time = dur;
				mtk_isr_point->isr_count = 1;
				mtk_isr_point->next = NULL;
				if (mtk_isr_current == NULL)
					current->se.mtk_isr = mtk_isr_point;
				else
					mtk_isr_current->next  = mtk_isr_point;

				isr_name = kmalloc(sizeof(action->name), GFP_ATOMIC);
				if (isr_name != NULL) {
					strcpy(isr_name, action->name);
					mtk_isr_point->isr_name = isr_name;
				} else {
					pr_debug("cant' alloc isr_name mem!\n");
				}
				current->se.mtk_isr_count++;
			}
		}
		return;
	}
#endif
	/* only record dur in mtk_isr_time if:
	 * CONFIG_MTPROF_CPUTIME defined but not enabled, or
	 * CONFIG_MTPROF_CPUTIME not defined
	 */
	if ((current->policy == SCHED_FIFO || current->policy == SCHED_RR)
	    && mt_rt_mon_enable(smp_processor_id()))
		current->se.mtk_isr_time += dur;
}
#endif

irqreturn_t
handle_irq_event_percpu(struct irq_desc *desc, struct irqaction *action)
{
	irqreturn_t retval = IRQ_NONE;
	unsigned int flags = 0, irq = desc->irq_data.irq;
#if defined(CONFIG_MTPROF_CPUTIME) || defined(CONFIG_MT_RT_THROTTLE_MON)
	unsigned long long t1, t2;
#endif

	do {
		irqreturn_t res;

		trace_irq_handler_entry(irq, action);
#if defined(CONFIG_MTPROF_CPUTIME) || defined(CONFIG_MT_RT_THROTTLE_MON)
		t1 = sched_clock();
#endif
		res = action->handler(irq, action->dev_id);
#if defined(CONFIG_MTPROF_CPUTIME) || defined(CONFIG_MT_RT_THROTTLE_MON)
		t2 = sched_clock();
		save_isr_info(irq, action, t1, t2);
#endif
		trace_irq_handler_exit(irq, action, res);

		if (WARN_ONCE(!irqs_disabled(), "irq %u handler %pF enabled interrupts\n",
			      irq, action->handler))
			local_irq_disable();

		switch (res) {
		case IRQ_WAKE_THREAD:
			/*
			 * Catch drivers which return WAKE_THREAD but
			 * did not set up a thread function
			 */
			if (unlikely(!action->thread_fn)) {
				warn_no_thread(irq, action);
				break;
			}

			__irq_wake_thread(desc, action);

			/* Fall through to add to randomness */
		case IRQ_HANDLED:
			flags |= action->flags;
			break;

		default:
			break;
		}

		retval |= res;
		action = action->next;
	} while (action);

	add_interrupt_randomness(irq, flags);

	if (!noirqdebug)
		note_interrupt(irq, desc, retval);
	return retval;
}

irqreturn_t handle_irq_event(struct irq_desc *desc)
{
	struct irqaction *action = desc->action;
	irqreturn_t ret;

	desc->istate &= ~IRQS_PENDING;
	irqd_set(&desc->irq_data, IRQD_IRQ_INPROGRESS);
	raw_spin_unlock(&desc->lock);

	ret = handle_irq_event_percpu(desc, action);

	raw_spin_lock(&desc->lock);
	irqd_clear(&desc->irq_data, IRQD_IRQ_INPROGRESS);
	return ret;
}
