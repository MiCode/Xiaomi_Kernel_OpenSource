/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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

#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <uapi/linux/sched/types.h>

#include "sprd_actions_queue.h"

/* The size must be Power of 2 */
#define ACTIONS_QUEUE_SIZE 64

struct sprd_actions_queue {
	u32	rdpos;
	u32	wtpos;
	u32	actions[ACTIONS_QUEUE_SIZE];
	int	params[ACTIONS_QUEUE_SIZE];
	int	sched_priority;

	do_actions	do_function;
	void		*data;

	/* use for protect the actions member. */
	spinlock_t	action_lock;

	char			*name;
	wait_queue_head_t	action_wait;
	struct task_struct	*thread;
};

int sprd_add_action_ex(void *actions_queue, u32 action, int param)
{
	struct sprd_actions_queue *aq =
		(struct sprd_actions_queue *)actions_queue;
	unsigned long flags;
	int ret = -EBUSY;
	u32 pos;

	spin_lock_irqsave(&aq->action_lock, flags);
	if (aq->rdpos == aq->wtpos ||
		(aq->wtpos & (ACTIONS_QUEUE_SIZE - 1)) != aq->rdpos) {
		ret = 0;
		pos = aq->wtpos & (ACTIONS_QUEUE_SIZE - 1);
		aq->actions[pos] = action;
		aq->params[pos] = param;
		aq->wtpos++;
	}
	spin_unlock_irqrestore(&aq->action_lock, flags);

	if (ret) {
		pr_err_once("%s queue is full,", aq->name);
		return ret;
	}

	wake_up_interruptible_all(&aq->action_wait);

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_add_action_ex);

static int sprd_action_thread(void *data)
{
	struct sprd_actions_queue *aq = (struct sprd_actions_queue *)data;
	u32 pos;
	unsigned long flags;

	while (!kthread_should_stop()) {
		wait_event_interruptible(aq->action_wait,
					  aq->rdpos != aq->wtpos);

		while (aq->rdpos != aq->wtpos) {
			pos = aq->rdpos & (ACTIONS_QUEUE_SIZE - 1);
			aq->do_function(aq->actions[pos],
					aq->params[pos], aq->data);
			spin_lock_irqsave(&aq->action_lock, flags);
			aq->rdpos++;
			spin_unlock_irqrestore(&aq->action_lock, flags);
		}
	}

	return 0;
}

void *sprd_create_action_queue(char *name, do_actions do_function,
			       void *data, int sched_priority)
{
	struct sprd_actions_queue *aq;
	struct sched_param param = {.sched_priority = sched_priority};

	if (!do_function || !name)
		return NULL;

	aq = kzalloc(sizeof(*aq), GFP_KERNEL);
	if (!aq)
		return NULL;

	aq->thread = kthread_create(sprd_action_thread, aq, name);
	if (IS_ERR(aq->thread)) {
		pr_err("Failed to create kthread: %s\n", name);
		kfree(aq);
		return NULL;
	}

	aq->name = name;
	aq->do_function = do_function;
	aq->data = data;

	spin_lock_init(&aq->action_lock);
	init_waitqueue_head(&aq->action_wait);

	sched_setscheduler(aq->thread, SCHED_FIFO, &param);
	wake_up_process(aq->thread);

	return aq;
}
EXPORT_SYMBOL_GPL(sprd_create_action_queue);

void sprd_destroy_action_queue(void *action_queue)
{
	struct sprd_actions_queue *aq =
		(struct sprd_actions_queue *)action_queue;

	if (!IS_ERR_OR_NULL(aq->thread))
		kthread_stop(aq->thread);

	kfree(aq);
}
EXPORT_SYMBOL_GPL(sprd_destroy_action_queue);
