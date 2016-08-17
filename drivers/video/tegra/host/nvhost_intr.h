/*
 * drivers/video/tegra/host/nvhost_intr.h
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

#ifndef __NVHOST_INTR_H
#define __NVHOST_INTR_H

#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

struct nvhost_channel;

enum nvhost_intr_action {
	/**
	 * Perform cleanup after a submit has completed.
	 * 'data' points to a channel
	 */
	NVHOST_INTR_ACTION_SUBMIT_COMPLETE = 0,

	/**
	 * Save a HW context.
	 * 'data' points to a context
	 */
	NVHOST_INTR_ACTION_CTXSAVE,

	/**
	 * Wake up a  task.
	 * 'data' points to a wait_queue_head_t
	 */
	NVHOST_INTR_ACTION_WAKEUP,

	/**
	 * Wake up a interruptible task.
	 * 'data' points to a wait_queue_head_t
	 */
	NVHOST_INTR_ACTION_WAKEUP_INTERRUPTIBLE,

	NVHOST_INTR_ACTION_COUNT
};

struct nvhost_intr;

struct nvhost_intr_syncpt {
	struct nvhost_intr *intr;
	u8 id;
	u16 irq;
	spinlock_t lock;
	struct list_head wait_head;
	char thresh_irq_name[12];
	struct work_struct work;
};

struct nvhost_intr {
	struct nvhost_intr_syncpt *syncpt;
	struct mutex mutex;
	int host_general_irq;
	int host_syncpt_irq_base;
	struct workqueue_struct *wq;
	void (*generic_isr[BITS_PER_LONG])(void);
	void (*generic_isr_thread[BITS_PER_LONG])(void);
	u32 intstatus;
};
#define intr_to_dev(x) container_of(x, struct nvhost_master, intr)
#define intr_syncpt_to_intr(is) (is->intr)

/**
 * Schedule an action to be taken when a sync point reaches the given threshold.
 *
 * @id the sync point
 * @thresh the threshold
 * @action the action to take
 * @data a pointer to extra data depending on action, see above
 * @waiter waiter allocated with nvhost_intr_alloc_waiter - assumes ownership
 * @ref must be passed if cancellation is possible, else NULL
 *
 * This is a non-blocking api.
 */
int nvhost_intr_add_action(struct nvhost_intr *intr, u32 id, u32 thresh,
			enum nvhost_intr_action action, void *data,
			void *waiter,
			void **ref);

/**
 * Allocate a waiter.
 */
void *nvhost_intr_alloc_waiter(void);

/**
 * Unreference an action submitted to nvhost_intr_add_action().
 * You must call this if you passed non-NULL as ref.
 * @ref the ref returned from nvhost_intr_add_action()
 */
void nvhost_intr_put_ref(struct nvhost_intr *intr, u32 id, void *ref);

int nvhost_intr_init(struct nvhost_intr *intr, u32 irq_gen, u32 irq_sync);
void nvhost_intr_deinit(struct nvhost_intr *intr);
void nvhost_intr_start(struct nvhost_intr *intr, u32 hz);
void nvhost_intr_stop(struct nvhost_intr *intr);

irqreturn_t nvhost_syncpt_thresh_fn(int irq, void *dev_id);
irqreturn_t nvhost_intr_irq_fn(int irq, void *dev_id);

void nvhost_intr_enable_general_irq(struct nvhost_intr *intr, int irq,
	void (*generic_isr)(void), void (*generic_isr_thread));
void nvhost_intr_disable_general_irq(struct nvhost_intr *intr, int irq);

#endif
