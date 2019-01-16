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
#define TIME_6MS 6000000
#define TIME_3MS 3000000
#ifdef CONFIG_MTPROF_CPUTIME
/*  cputime monitor en/disable value */
extern int mtsched_enabled;
#ifdef CONFIG_MT_ENG_BUILD
#define MAX_THREAD_COUNT 6000	// max debug thread count, if reach the level, stop store new thread informaiton.
#else
#define MAX_THREAD_COUNT 3000
#endif
extern int proc_count;
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

static void warn_no_thread(unsigned int irq, struct irqaction *action)
{
	if (test_and_set_bit(IRQTF_WARNED, &action->thread_flags))
		return;

	printk(KERN_WARNING "IRQ %d device %s returned IRQ_WAKE_THREAD "
	       "but no thread function available.", irq, action->name);
}

static void irq_wake_thread(struct irq_desc *desc, struct irqaction *action)
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

irqreturn_t
handle_irq_event_percpu(struct irq_desc *desc, struct irqaction *action)
{
	irqreturn_t retval = IRQ_NONE;
	unsigned int flags = 0, irq = desc->irq_data.irq;

#ifdef CONFIG_MTPROF_IRQ_DURATION
	unsigned long long t1, t2, dur;
#endif
	do {
		irqreturn_t res;

		trace_irq_handler_entry(irq, action);
#ifdef CONFIG_MTPROF_IRQ_DURATION
		t1 = sched_clock();
		res = action->handler(irq, action->dev_id);
		t2 = sched_clock();
		dur = t2 - t1;
		action->duration += dur;
		action->count++;
		action->dur_max = max(dur,action->dur_max);
		action->dur_min = min(dur,action->dur_min);
#ifdef CONFIG_MTPROF_CPUTIME
		if(mtsched_enabled == 1)
		{
			int isr_find = 0;
			struct mtk_isr_info *mtk_isr_point = current->se.mtk_isr;
			struct mtk_isr_info *mtk_isr_current = mtk_isr_point;
			char *isr_name = NULL;
			
			current->se.mtk_isr_time += dur;
			while((current->se.mtk_isr!= NULL) && (mtk_isr_point != NULL))
			{
				if(mtk_isr_point->isr_num == irq)
				{
					mtk_isr_point->isr_time += dur;
					mtk_isr_point->isr_count++;
					isr_find = 1;
					break;
				}
				mtk_isr_current = mtk_isr_point;
				mtk_isr_point = mtk_isr_point -> next;
			}

			if((isr_find == 0) && (proc_count < MAX_THREAD_COUNT))
			{
				mtk_isr_point =  kmalloc(sizeof(struct mtk_isr_info), GFP_ATOMIC);
				if(mtk_isr_point == NULL)
				{
					printk(KERN_ERR"cant' alloc mtk_isr_info mem!\n");					
				}
				else
				{
					mtk_isr_point->isr_num = irq;
					mtk_isr_point->isr_time = dur;
					mtk_isr_point->isr_count = 1;
					mtk_isr_point->next = NULL;
					if(mtk_isr_current == NULL)
					{
						current->se.mtk_isr = mtk_isr_point;
					}
					else
					{
						mtk_isr_current->next  = mtk_isr_point;
					}

					isr_name = kmalloc(sizeof(action->name),GFP_ATOMIC);
					if(isr_name != NULL)
					{
						strcpy(isr_name, action->name);
						mtk_isr_point->isr_name = isr_name;
					}
					else
					{
						printk(KERN_ERR"cant' alloc isr_name mem!\n");
					}
					current->se.mtk_isr_count++;
				}	
			}
		}
#endif		
#else
		res = action->handler(irq, action->dev_id);
#endif
		trace_irq_handler_exit(irq, action, res);

		if (WARN_ONCE(!irqs_disabled(),"irq %u handler %pF enabled interrupts\n",
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

			irq_wake_thread(desc, action);

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
