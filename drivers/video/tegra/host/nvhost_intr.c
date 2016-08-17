/*
 * drivers/video/tegra/host/nvhost_intr.c
 *
 * Tegra Graphics Host Interrupt Management
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nvhost_intr.h"
#include "dev.h"
#include "nvhost_acm.h"
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <trace/events/nvhost.h>
#include "nvhost_channel.h"
#include "nvhost_hwctx.h"
#include "chip_support.h"

/*** Wait list management ***/

struct nvhost_waitlist {
	struct list_head list;
	struct kref refcount;
	u32 thresh;
	enum nvhost_intr_action action;
	atomic_t state;
	void *data;
	int count;
};

enum waitlist_state {
	WLS_PENDING,
	WLS_REMOVED,
	WLS_CANCELLED,
	WLS_HANDLED
};

static void waiter_release(struct kref *kref)
{
	kfree(container_of(kref, struct nvhost_waitlist, refcount));
}

/**
 * add a waiter to a waiter queue, sorted by threshold
 * returns true if it was added at the head of the queue
 */
static bool add_waiter_to_queue(struct nvhost_waitlist *waiter,
				struct list_head *queue)
{
	struct nvhost_waitlist *pos;
	u32 thresh = waiter->thresh;

	list_for_each_entry_reverse(pos, queue, list)
		if ((s32)(pos->thresh - thresh) <= 0) {
			list_add(&waiter->list, &pos->list);
			return false;
		}

	list_add(&waiter->list, queue);
	return true;
}

/**
 * run through a waiter queue for a single sync point ID
 * and gather all completed waiters into lists by actions
 */
static void remove_completed_waiters(struct list_head *head, u32 sync,
			struct list_head completed[NVHOST_INTR_ACTION_COUNT])
{
	struct list_head *dest;
	struct nvhost_waitlist *waiter, *next, *prev;

	list_for_each_entry_safe(waiter, next, head, list) {
		if ((s32)(waiter->thresh - sync) > 0)
			break;

		dest = completed + waiter->action;

		/* consolidate submit cleanups */
		if (waiter->action == NVHOST_INTR_ACTION_SUBMIT_COMPLETE
			&& !list_empty(dest)) {
			prev = list_entry(dest->prev,
					struct nvhost_waitlist, list);
			if (prev->data == waiter->data) {
				prev->count++;
				dest = NULL;
			}
		}

		/* PENDING->REMOVED or CANCELLED->HANDLED */
		if (atomic_inc_return(&waiter->state) == WLS_HANDLED || !dest) {
			list_del(&waiter->list);
			kref_put(&waiter->refcount, waiter_release);
		} else {
			list_move_tail(&waiter->list, dest);
		}
	}
}

void reset_threshold_interrupt(struct nvhost_intr *intr,
			       struct list_head *head,
			       unsigned int id)
{
	u32 thresh = list_first_entry(head,
				struct nvhost_waitlist, list)->thresh;
	BUG_ON(!(intr_op().set_syncpt_threshold &&
		 intr_op().enable_syncpt_intr));

	intr_op().set_syncpt_threshold(intr, id, thresh);
	intr_op().enable_syncpt_intr(intr, id);
}


static void action_submit_complete(struct nvhost_waitlist *waiter)
{
	struct nvhost_channel *channel = waiter->data;
	int nr_completed = waiter->count;

	nvhost_module_idle_mult(channel->dev, nr_completed);
	nvhost_cdma_update(&channel->cdma);

	/*  Add nr_completed to trace */
	trace_nvhost_channel_submit_complete(channel->dev->name,
			nr_completed, waiter->thresh,
			channel->cdma.high_prio_count,
			channel->cdma.med_prio_count,
			channel->cdma.low_prio_count);

}

static void action_ctxsave(struct nvhost_waitlist *waiter)
{
	struct nvhost_hwctx *hwctx = waiter->data;
	struct nvhost_channel *channel = hwctx->channel;

	if (channel->ctxhandler->save_service)
		channel->ctxhandler->save_service(hwctx);
}

static void action_wakeup(struct nvhost_waitlist *waiter)
{
	wait_queue_head_t *wq = waiter->data;

	wake_up(wq);
}

static void action_wakeup_interruptible(struct nvhost_waitlist *waiter)
{
	wait_queue_head_t *wq = waiter->data;

	wake_up_interruptible(wq);
}

typedef void (*action_handler)(struct nvhost_waitlist *waiter);

static action_handler action_handlers[NVHOST_INTR_ACTION_COUNT] = {
	action_submit_complete,
	action_ctxsave,
	action_wakeup,
	action_wakeup_interruptible,
};

static void run_handlers(struct list_head completed[NVHOST_INTR_ACTION_COUNT])
{
	struct list_head *head = completed;
	int i;

	for (i = 0; i < NVHOST_INTR_ACTION_COUNT; ++i, ++head) {
		action_handler handler = action_handlers[i];
		struct nvhost_waitlist *waiter, *next;

		list_for_each_entry_safe(waiter, next, head, list) {
			list_del(&waiter->list);
			handler(waiter);
			WARN_ON(atomic_xchg(&waiter->state, WLS_HANDLED) != WLS_REMOVED);
			kref_put(&waiter->refcount, waiter_release);
		}
	}
}

/**
 * Remove & handle all waiters that have completed for the given syncpt
 */
static int process_wait_list(struct nvhost_intr *intr,
			     struct nvhost_intr_syncpt *syncpt,
			     u32 threshold)
{
	struct list_head completed[NVHOST_INTR_ACTION_COUNT];
	unsigned int i;
	int empty;

	for (i = 0; i < NVHOST_INTR_ACTION_COUNT; ++i)
		INIT_LIST_HEAD(completed + i);

	spin_lock(&syncpt->lock);

	remove_completed_waiters(&syncpt->wait_head, threshold, completed);

	empty = list_empty(&syncpt->wait_head);
	if (empty)
		intr_op().disable_syncpt_intr(intr, syncpt->id);
	else
		reset_threshold_interrupt(intr, &syncpt->wait_head,
					  syncpt->id);

	spin_unlock(&syncpt->lock);

	run_handlers(completed);

	return empty;
}

/*** host syncpt interrupt service functions ***/
/**
 * Sync point threshold interrupt service thread function
 * Handles sync point threshold triggers, in thread context
 */
irqreturn_t nvhost_syncpt_thresh_fn(int irq, void *dev_id)
{
	struct nvhost_intr_syncpt *syncpt = dev_id;
	unsigned int id = syncpt->id;
	struct nvhost_intr *intr = intr_syncpt_to_intr(syncpt);
	struct nvhost_master *dev = intr_to_dev(intr);

	(void)process_wait_list(intr, syncpt,
				nvhost_syncpt_update_min(&dev->syncpt, id));

	return IRQ_HANDLED;
}

irqreturn_t nvhost_intr_irq_fn(int irq, void *dev_id)
{
	struct nvhost_intr *intr = dev_id;
	unsigned long intstat = intr->intstatus;
	int i;

	for_each_set_bit(i, &intstat, BITS_PER_LONG) {
		if (intr->generic_isr_thread[i])
			intr->generic_isr_thread[i]();
	}

	return IRQ_HANDLED;
}

/*** host general interrupt service functions ***/


/*** Main API ***/

int nvhost_intr_add_action(struct nvhost_intr *intr, u32 id, u32 thresh,
			enum nvhost_intr_action action, void *data,
			void *_waiter,
			void **ref)
{
	struct nvhost_waitlist *waiter = _waiter;
	struct nvhost_intr_syncpt *syncpt;
	int queue_was_empty;

	BUG_ON(waiter == NULL);

	BUG_ON(!(intr_op().set_syncpt_threshold &&
		 intr_op().enable_syncpt_intr));

	/* initialize a new waiter */
	INIT_LIST_HEAD(&waiter->list);
	kref_init(&waiter->refcount);
	if (ref)
		kref_get(&waiter->refcount);
	waiter->thresh = thresh;
	waiter->action = action;
	atomic_set(&waiter->state, WLS_PENDING);
	waiter->data = data;
	waiter->count = 1;

	syncpt = intr->syncpt + id;

	spin_lock(&syncpt->lock);

	queue_was_empty = list_empty(&syncpt->wait_head);

	if (add_waiter_to_queue(waiter, &syncpt->wait_head)) {
		/* added at head of list - new threshold value */
		intr_op().set_syncpt_threshold(intr, id, thresh);

		/* added as first waiter - enable interrupt */
		if (queue_was_empty)
			intr_op().enable_syncpt_intr(intr, id);
	}

	spin_unlock(&syncpt->lock);

	if (ref)
		*ref = waiter;
	return 0;
}

void *nvhost_intr_alloc_waiter()
{
	return kzalloc(sizeof(struct nvhost_waitlist),
			GFP_KERNEL|__GFP_REPEAT);
}

void nvhost_intr_put_ref(struct nvhost_intr *intr, u32 id, void *ref)
{
	struct nvhost_waitlist *waiter = ref;
	struct nvhost_intr_syncpt *syncpt;
	struct nvhost_master *host = intr_to_dev(intr);

	while (atomic_cmpxchg(&waiter->state,
				WLS_PENDING, WLS_CANCELLED) == WLS_REMOVED)
		schedule();

	syncpt = intr->syncpt + id;
	(void)process_wait_list(intr, syncpt,
				nvhost_syncpt_update_min(&host->syncpt, id));

	kref_put(&waiter->refcount, waiter_release);
}


/*** Init & shutdown ***/

int nvhost_intr_init(struct nvhost_intr *intr, u32 irq_gen, u32 irq_sync)
{
	unsigned int id;
	struct nvhost_intr_syncpt *syncpt;
	struct nvhost_master *host = intr_to_dev(intr);
	u32 nb_pts = nvhost_syncpt_nb_pts(&host->syncpt);

	mutex_init(&intr->mutex);
	intr->host_syncpt_irq_base = irq_sync;
	intr->wq = create_workqueue("host_syncpt");
	intr_op().init_host_sync(intr);
	intr->host_general_irq = irq_gen;
	intr_op().request_host_general_irq(intr);

	for (id = 0, syncpt = intr->syncpt;
	     id < nb_pts;
	     ++id, ++syncpt) {
		syncpt->intr = &host->intr;
		syncpt->id = id;
		syncpt->irq = irq_sync + id;
		spin_lock_init(&syncpt->lock);
		INIT_LIST_HEAD(&syncpt->wait_head);
		snprintf(syncpt->thresh_irq_name,
			sizeof(syncpt->thresh_irq_name),
			"host_sp_%02d", id);
	}

	return 0;
}

void nvhost_intr_deinit(struct nvhost_intr *intr)
{
	nvhost_intr_stop(intr);
	destroy_workqueue(intr->wq);
}

void nvhost_intr_start(struct nvhost_intr *intr, u32 hz)
{
	BUG_ON(!(intr_op().init_host_sync &&
		 intr_op().set_host_clocks_per_usec &&
		 intr_op().request_host_general_irq));

	mutex_lock(&intr->mutex);

	intr_op().init_host_sync(intr);
	intr_op().set_host_clocks_per_usec(intr,
					       (hz + 1000000 - 1)/1000000);

	intr_op().request_host_general_irq(intr);

	mutex_unlock(&intr->mutex);
}

void nvhost_intr_stop(struct nvhost_intr *intr)
{
	unsigned int id;
	struct nvhost_intr_syncpt *syncpt;
	u32 nb_pts = nvhost_syncpt_nb_pts(&intr_to_dev(intr)->syncpt);

	BUG_ON(!(intr_op().disable_all_syncpt_intrs &&
		 intr_op().free_host_general_irq &&
		 intr_op().free_syncpt_irq));

	mutex_lock(&intr->mutex);

	intr_op().disable_all_syncpt_intrs(intr);

	for (id = 0, syncpt = intr->syncpt;
	     id < nb_pts;
	     ++id, ++syncpt) {
		struct nvhost_waitlist *waiter, *next;
		list_for_each_entry_safe(waiter, next, &syncpt->wait_head, list) {
			if (atomic_cmpxchg(&waiter->state, WLS_CANCELLED, WLS_HANDLED)
				== WLS_CANCELLED) {
				list_del(&waiter->list);
				kref_put(&waiter->refcount, waiter_release);
			}
		}

		if (!list_empty(&syncpt->wait_head)) {  /* output diagnostics */
			printk(KERN_DEBUG "%s id=%d\n", __func__, id);
			BUG_ON(1);
		}
	}

	intr_op().free_host_general_irq(intr);
	intr_op().free_syncpt_irq(intr);

	mutex_unlock(&intr->mutex);
}

void nvhost_intr_enable_general_irq(struct nvhost_intr *intr, int irq,
	void (*generic_isr)(void), void (*generic_isr_thread))
{
	intr->generic_isr[irq] = generic_isr;
	intr->generic_isr_thread[irq] = generic_isr_thread;
	intr_op().enable_general_irq(intr, irq);
}

void nvhost_intr_disable_general_irq(struct nvhost_intr *intr, int irq)
{
	intr_op().disable_general_irq(intr, irq);
}
