/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Implementation of Host Target Communication tasks,
 * WorkTask and compl_task, which are used to manage
 * the Mbox Pending Queues.
 *
 * A mailbox Send request is queued in arrival order on
 * a per-mailbox Send queue until a credit is available
 * from the Target. Requests in this queue are
 * waiting for the Target to provide tx credits (i.e. recv
 * buffers on the Target-side).
 *
 * A mailbox Recv request is queued in arrival order on
 * a per-mailbox Recv queue until a message is available
 * to be read. So requests in this queue are waiting for
 * the Target to provide rx data.
 *
 * htca_work_task dequeues requests from the SendPendingQueue
 * (once credits are available) and dequeues requests from
 * the RecvPendingQueue (once rx data is available) and
 * hands them to HIF for processing.
 *
 * htca_compl_task handles completion processing after
 * HIF completes a request.
 *
 * The main purpose of these tasks is to provide a
 * suitable suspendable context for processing requests
 * and completions.
 */

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>

#include "../hif_sdio/hif.h"
#include "htca.h"
#include "htca_mbox_internal.h"

/* Wakeup the htca_work_task.
 *
 * Invoked whenever send/recv state changes:
 * new Send buffer added to the send_pending_queue
 * new Recv buffer added to the recv_pending_queue
 * tx credits are reaped
 * rx data available recognized
 */
void htca_work_task_poke(struct htca_target *target)
{
	target->work_task_has_work = true;
	wake_up_interruptible_sync(&target->work_task_wait);
}

/* Body of the htca_work_task, which hands Send and
 * Receive requests to HIF.
 */
static int htca_work_task_core(struct htca_target *target)
{
	int ep;
	int work_done = 0;

	/* TBD: We might consider alternative ordering policies, here,
	 * between Sends and Recvs and among mailboxes. The current
	 * algorithm is simple.
	 */

	/* Process sends/recvs */
	for (ep = 0; ep < HTCA_NUM_MBOX; ep++) {
		htcadebug("Call (%d)\n", ep);
		work_done += htca_manage_pending_sends(target, ep);
		htcadebug("Call (%d)\n", ep);
		work_done += htca_manage_pending_recvs(target, ep);
	}

	return work_done;
}

/* Only this work_task is permitted to update
 * interrupt enables. That restriction eliminates
 * complex race conditions.
 */
static int htca_work_task(void *param)
{
	struct htca_target *target = (struct htca_target *)param;

	/* set_user_nice(current, -3); */
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		htcadebug("top of loop. intr_state=%d\n", target->intr_state);
		/* Wait for htca_work_task_poke */
		wait_event_interruptible(target->work_task_wait,
					 target->work_task_has_work);

		if (target->work_task_shutdown)
			break; /* htcaTaskStop invoked */

		if (!target->work_task_has_work)
			break; /* exit, if this task was interrupted */

		/* reset before we start work */
		target->work_task_has_work = false;
		barrier();

		if (target->need_start_polling) {
			/* reset for next time */
			target->need_start_polling = 0;
			target->intr_state = HTCA_POLL;
			htca_update_intr_enbs(target, 1);
		}

		while (htca_work_task_core(target))
			;

		if (target->pending_recv_mask ||
		    target->pending_register_refresh) {
			continue;
		}

		/* When a Recv completes, it sets need_register_refresh=1
		 * and pokes the work_task.
		 *
		 * We won't actually initiate a register refresh until
		 * pending recvs on ALL eps have completed. This may
		 * increase latency slightly but it increases efficiency
		 * and reduces chatter which should improve throughput.
		 * Note that even though we don't initiate the register
		 * refresh immediately, SDIO is still 100% busy doing
		 * useful work. The refresh is issued shortly after.
		 */
		if (target->need_register_refresh) {
			/* Continue to poll. When the RegsiterRefresh
			 * completes, the WorkTask will be poked.
			 */
			target->need_register_refresh = 0;
			htca_register_refresh_start(target);
			continue;
		}

		/* If more work has arrived since we last checked,
		 * make another pass.
		 */
		if (target->work_task_has_work)
			continue;

		/* As long as we are constantly refreshing register
		 * state and reprocessing, there is no need to
		 * enable interrupts. We are essentially POLLING for
		 * interrupts anyway. But if
		 * -we were in POLL mode and
		 * -we have processed all outstanding sends/recvs and
		 * -there are no PENDING recv operations and
		 * -there is no pending register refresh (so
		 * no recv operations have completed since the
		 * last time we refreshed register state)
		 * then we switch to INTERRUPT mode and re-enable
		 * Target-side interrupts.
		 *
		 * We'll sleep until poked:
		 * -DSR handler receives an interrupt
		 * -application enqueues a new send/recv buffer
		 * We must also UPDATE interrupt enables even if we
		 * were already in INTERRUPT mode, since some bits
		 * may have changed.
		 */
		if (target->intr_state == HTCA_POLL) {
			target->intr_state = HTCA_INTERRUPT;
			htca_update_intr_enbs(target, 0);
		}
	}
	complete_and_exit(&target->work_task_completion, 0);

	return 0;
}

int htca_work_task_start(struct htca_target *target)
{
	int status = HTCA_ERROR;

	if (mutex_lock_interruptible(&target->task_mutex))
		return HTCA_ERROR; /* interrupted */

	if (target->work_task)
		goto done; /* already started */

	target->work_task = kthread_create(htca_work_task, target, "htcaWork");
	if (!target->work_task)
		goto done; /* Failed to create task */

	target->work_task_shutdown = false;
	init_waitqueue_head(&target->work_task_wait);
	init_completion(&target->work_task_completion);
	wake_up_process(target->work_task);
	status = HTCA_OK;

done:
	mutex_unlock(&target->task_mutex);
	return status;
}

void htca_work_task_stop(struct htca_target *target)
{
	if (mutex_lock_interruptible(&target->task_mutex))
		return; /* interrupted */

	if (!target->work_task)
		goto done;

	target->work_task_shutdown = true;
	htca_work_task_poke(target);
	wait_for_completion(&target->work_task_completion);
	target->work_task = NULL;

done:
	mutex_unlock(&target->task_mutex);
}

/* Wakeup the compl_task.
 * Invoked after adding a new completion to the compl_queue.
 */
void htca_compl_task_poke(struct htca_target *target)
{
	target->compl_task_has_work = true;
	wake_up_interruptible_sync(&target->compl_task_wait);
}

static int htca_manage_compl(struct htca_target *target)
{
	struct htca_request *req;
	unsigned long flags;

	/* Pop a request from the completion queue */
	spin_lock_irqsave(&target->compl_queue_lock, flags);
	req = htca_request_deq_head(&target->compl_queue);
	spin_unlock_irqrestore(&target->compl_queue_lock, flags);

	if (!req)
		return 0; /* nothing to do */

	/* Invoke request's corresponding completion function */
	if (req->completion_cb)
		req->completion_cb(req, req->status);

	return 1;
}

static int htca_compl_task(void *param)
{
	struct htca_target *target = (struct htca_target *)param;

	/* set_user_nice(current, -3); */
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		/* Wait for htca_compl_task_poke */
		wait_event_interruptible(target->compl_task_wait,
					 target->compl_task_has_work);
		if (target->compl_task_shutdown)
			break; /* htcaTaskStop invoked */

		if (!target->compl_task_has_work)
			break; /* exit, if this task was interrupted */

		/* reset before we start work */
		target->compl_task_has_work = false;
		barrier();

		/* TBD: We could try to prioritize completions rather than
		 * handle them strictly in order. Could use separate queues for
		 * register completions and mailbox completion on each endpoint.
		 * In general, completion processing is expected to be short
		 * so this probably isn't worth the additional complexity.
		 */
		{
			int did_work;

			do {
				did_work = htca_manage_compl(target);
			} while (did_work);
		}
	}
	complete_and_exit(&target->compl_cask_completion, 0);

	return 0;
}

int htca_compl_task_start(struct htca_target *target)
{
	int status = HTCA_ERROR;

	if (mutex_lock_interruptible(&target->task_mutex))
		return HTCA_ERROR; /* interrupted */

	if (target->compl_task)
		goto done; /* already started */

	target->compl_task =
	    kthread_create(htca_compl_task, target, "htcaCompl");
	if (!target->compl_task)
		goto done; /* Failed to create task */

	target->compl_task_shutdown = false;
	init_waitqueue_head(&target->compl_task_wait);
	init_completion(&target->compl_cask_completion);
	wake_up_process(target->compl_task);
	status = HTCA_OK;

done:
	mutex_unlock(&target->task_mutex);
	return status;
}

void htca_compl_task_stop(struct htca_target *target)
{
	if (mutex_lock_interruptible(&target->task_mutex))
		return; /* interrupted */

	if (!target->compl_task)
		goto done;

	target->compl_task_shutdown = true;
	htca_compl_task_poke(target);
	wait_for_completion(&target->compl_cask_completion);
	target->compl_task = NULL;

done:
	mutex_unlock(&target->task_mutex);
}
