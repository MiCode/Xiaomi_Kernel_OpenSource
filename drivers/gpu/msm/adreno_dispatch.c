/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/jiffies.h>

#include "kgsl.h"
#include "adreno.h"
#include "adreno_ringbuffer.h"
#include "adreno_trace.h"

#define ADRENO_DISPATCHER_ACTIVE 0
#define ADRENO_DISPATCHER_PAUSE 1

#define CMDQUEUE_NEXT(_i, _s) (((_i) + 1) % (_s))

/* Number of commands that can be queued in a context before it sleeps */
static unsigned int _context_cmdqueue_size = 50;

/* Number of milliseconds to wait for the context queue to clear */
static unsigned int _context_queue_wait = 10000;

/* Number of command batches sent at a time from a single context */
static unsigned int _context_cmdbatch_burst = 5;

/* Number of command batches inflight in the ringbuffer at any time */
static unsigned int _dispatcher_inflight = 15;

/* Command batch timeout (in milliseconds) */
static unsigned int _cmdbatch_timeout = 2000;

/**
 * adreno_dispatcher_get_cmdbatch() - Get a new command from a context queue
 * @drawctxt: Pointer to the adreno draw context
 *
 * Dequeue a new command batch from the context list
 */
static inline struct kgsl_cmdbatch *adreno_dispatcher_get_cmdbatch(
		struct adreno_context *drawctxt)
{
	struct kgsl_cmdbatch *cmdbatch = NULL;

	mutex_lock(&drawctxt->mutex);
	if (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		cmdbatch = drawctxt->cmdqueue[drawctxt->cmdqueue_head];
		drawctxt->cmdqueue_head =
			CMDQUEUE_NEXT(drawctxt->cmdqueue_head,
			ADRENO_CONTEXT_CMDQUEUE_SIZE);
		drawctxt->queued--;
	}

	mutex_unlock(&drawctxt->mutex);

	return cmdbatch;
}

/**
 * adreno_dispatcher_requeue_cmdbatch() - Put a command back on the context
 * queue
 * @drawctxt: Pointer to the adreno draw context
 * @cmdbatch: Pointer to the KGSL cmdbatch to requeue
 *
 * Failure to submit a command to the ringbuffer isn't the fault of the command
 * being submitted so if a failure happens, push it back on the head of the the
 * context queue to be reconsidered again
 */
static inline void adreno_dispatcher_requeue_cmdbatch(
		struct adreno_context *drawctxt, struct kgsl_cmdbatch *cmdbatch)
{
	unsigned int prev;
	mutex_lock(&drawctxt->mutex);

	if (kgsl_context_detached(&drawctxt->base) ||
		drawctxt->state == ADRENO_CONTEXT_STATE_INVALID) {
		mutex_unlock(&drawctxt->mutex);
		return;
	}

	prev = drawctxt->cmdqueue_head - 1;

	if (prev < 0)
		prev = ADRENO_CONTEXT_CMDQUEUE_SIZE - 1;

	/*
	 * The maximum queue size always needs to be one less then the size of
	 * the ringbuffer queue so there is "room" to put the cmdbatch back in
	 */

	BUG_ON(prev == drawctxt->cmdqueue_tail);

	drawctxt->cmdqueue[prev] = cmdbatch;
	drawctxt->queued++;

	/* Reset the command queue head to reflect the newly requeued change */
	drawctxt->cmdqueue_head = prev;
	mutex_unlock(&drawctxt->mutex);
}

/**
 * dispatcher_queue_context() - Queue a context in the dispatcher pending list
 * @dispatcher: Pointer to the adreno dispatcher struct
 * @drawctxt: Pointer to the adreno draw context
 *
 * Add a context to the dispatcher pending list.
 */
static void  dispatcher_queue_context(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	spin_lock(&dispatcher->plist_lock);

	if (plist_node_empty(&drawctxt->pending)) {
		/* Get a reference to the context while it sits on the list */
		_kgsl_context_get(&drawctxt->base);
		trace_dispatch_queue_context(drawctxt);
		plist_add(&drawctxt->pending, &dispatcher->pending);
	}

	spin_unlock(&dispatcher->plist_lock);
}

/**
 * sendcmd() - Send a command batch to the GPU hardware
 * @dispatcher: Pointer to the adreno dispatcher struct
 * @cmdbatch: Pointer to the KGSL cmdbatch being sent
 *
 * Send a KGSL command batch to the GPU hardware
 */
static int sendcmd(struct adreno_device *adreno_dev,
	struct kgsl_cmdbatch *cmdbatch)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int ret;

	dispatcher->inflight++;

	mutex_lock(&device->mutex);

	if (dispatcher->inflight == 1) {
		/* Time to make the donuts.  Turn on the GPU */
		ret = kgsl_active_count_get(device);
		if (ret) {
			dispatcher->inflight--;
			mutex_unlock(&device->mutex);
			return ret;
		}
	}

	ret = adreno_ringbuffer_submitcmd(adreno_dev, cmdbatch);

	/* Turn the GPU back off on failure.  Sad face. */
	if (ret && dispatcher->inflight == 1)
		kgsl_active_count_put(device);

	mutex_unlock(&device->mutex);

	if (ret) {
		dispatcher->inflight--;
		KGSL_DRV_ERR(device,
			"Unable to submit command to the ringbuffer\n");
		return ret;
	}

	trace_adreno_cmdbatch_submitted(cmdbatch, dispatcher->inflight);

	dispatcher->cmdqueue[dispatcher->tail] = cmdbatch;
	dispatcher->tail = (dispatcher->tail + 1) %
		ADRENO_DISPATCH_CMDQUEUE_SIZE;

	/*
	 * If this is the first command in the pipe then the GPU will
	 * immediately start executing it so we can start the expiry timeout on
	 * the command batch here.  Subsequent command batches will have their
	 * timer started when the previous command batch is retired
	 */
	if (dispatcher->inflight == 1) {
		cmdbatch->expires = jiffies +
			msecs_to_jiffies(_cmdbatch_timeout);
		mod_timer(&dispatcher->timer, cmdbatch->expires);
	}

	return 0;
}

/**
 * dispatcher_context_sendcmds() - Send commands from a context to the GPU
 * @adreno_dev: Pointer to the adreno device struct
 * @drawctxt: Pointer to the adreno context to dispatch commands from
 *
 * Dequeue and send a burst of commands from the specified context to the GPU
 */
static int dispatcher_context_sendcmds(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int count = 0;

	/*
	 * Each context can send a specific number of command batches per cycle
	 */
	for ( ; count < _context_cmdbatch_burst &&
		dispatcher->inflight < _dispatcher_inflight; count++) {
		int ret;
		struct kgsl_cmdbatch *cmdbatch =
			adreno_dispatcher_get_cmdbatch(drawctxt);

		if (cmdbatch == NULL)
			break;

		ret = sendcmd(adreno_dev, cmdbatch);

		/*
		 * There are various reasons why we can't submit a command (no
		 * memory for the commands, full ringbuffer, etc) but none of
		 * these are actually the current command's fault.  Requeue it
		 * back on the context and let it come back around again if
		 * conditions improve
		 */
		if (ret) {
			adreno_dispatcher_requeue_cmdbatch(drawctxt, cmdbatch);
			break;
		}
	}

	/*
	 * If the context successfully submitted commands, then
	 * unconditionally put it back on the queue to be considered the
	 * next time around. This might seem a little wasteful but it is
	 * reasonable to think that a busy context will stay busy.
	 */

	if (count) {
		dispatcher_queue_context(adreno_dev, drawctxt);

		/*
		 * If we submitted something there will be room in the
		 * context queue so ping the context wait queue on the
		 * chance that the context is snoozing
		 */

		wake_up_interruptible_all(&drawctxt->wq);
	}

	return count;
}

/**
 * _adreno_dispatcher_issuecmds() - Issue commmands from pending contexts
 * @adreno_dev: Pointer to the adreno device struct
 *
 * Issue as many commands as possible (up to inflight) from the pending contexts
 * This function assumes the dispatcher mutex has been locked.
 */
static int _adreno_dispatcher_issuecmds(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	/* Don't do anything if the dispatcher is paused */
	if (dispatcher->state != ADRENO_DISPATCHER_ACTIVE)
		return 0;

	while (dispatcher->inflight < _dispatcher_inflight) {
		struct adreno_context *drawctxt = NULL;

		spin_lock(&dispatcher->plist_lock);

		if (!plist_head_empty(&dispatcher->pending)) {
			drawctxt = plist_first_entry(&dispatcher->pending,
				struct adreno_context, pending);

			plist_del(&drawctxt->pending, &dispatcher->pending);
		}

		spin_unlock(&dispatcher->plist_lock);

		if (drawctxt == NULL)
			break;

		if (kgsl_context_detached(&drawctxt->base) ||
			drawctxt->state == ADRENO_CONTEXT_STATE_INVALID) {
			kgsl_context_put(&drawctxt->base);
			continue;
		}

		dispatcher_context_sendcmds(adreno_dev, drawctxt);
		kgsl_context_put(&drawctxt->base);
	}

	return 0;
}

/**
 * adreno_dispatcher_issuecmds() - Issue commmands from pending contexts
 * @adreno_dev: Pointer to the adreno device struct
 *
 * Lock the dispatcher and call _adreno_dispatcher_issueibcmds
 */
int adreno_dispatcher_issuecmds(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int ret;

	mutex_lock(&dispatcher->mutex);
	ret = _adreno_dispatcher_issuecmds(adreno_dev);
	mutex_unlock(&dispatcher->mutex);

	return ret;
}

static int _check_context_queue(struct adreno_context *drawctxt)
{
	int ret;

	mutex_lock(&drawctxt->mutex);

	/*
	 * Wake up if there is room in the context or if the whole thing got
	 * invalidated while we were asleep
	 */

	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID)
		ret = 1;
	else
		ret = drawctxt->queued < _context_cmdqueue_size ? 1 : 0;

	mutex_unlock(&drawctxt->mutex);

	return ret;
}

/**
 * adreno_dispatcher_replay() - Replay commands from the dispatcher queue
 * @adreno_dev: Pointer to the adreno device struct
 *
 * Replay the commands from the dispatcher inflight queue.  This is called after
 * a power down/up to recover from a fault
 */
int adreno_dispatcher_replay(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct kgsl_cmdbatch **replay;
	int i, ptr, count = 0;

	BUG_ON(!mutex_is_locked(&dispatcher->mutex));

	replay = kzalloc(sizeof(*replay) * dispatcher->inflight, GFP_KERNEL);

	/*
	 * If we can't allocate enough memory for the replay commands then we
	 * are in a bad way.  Invalidate everything, reset the GPU and see ya
	 * later alligator
	 */

	if (replay == NULL) {

		ptr = dispatcher->head;

		while (ptr != dispatcher->tail) {
			struct kgsl_context *context =
				dispatcher->cmdqueue[ptr]->context;

			adreno_drawctxt_invalidate(device, context);
			ptr = CMDQUEUE_NEXT(ptr, ADRENO_DISPATCH_CMDQUEUE_SIZE);
		}

		/* Reset the dispatcher queue */
		dispatcher->inflight = 0;
		dispatcher->head = dispatcher->tail = 0;

		/* Reset the hardware */
		mutex_lock(&device->mutex);

		/*
		 * If adreno_reset fails then the GPU is not alive and there
		 * isn't anything we can do to recover at this point
		 */

		BUG_ON(adreno_reset(device));
		mutex_unlock(&device->mutex);

		return 0;
	}

	ptr = dispatcher->head;

	while (ptr != dispatcher->tail) {
		struct kgsl_cmdbatch *cmdbatch = dispatcher->cmdqueue[ptr];
		struct adreno_context *drawctxt =
			ADRENO_CONTEXT(cmdbatch->context);

		if (cmdbatch->invalid)
			adreno_drawctxt_invalidate(device, cmdbatch->context);

		if (!kgsl_context_detached(cmdbatch->context) &&
			drawctxt->state == ADRENO_CONTEXT_STATE_ACTIVE) {
			/*
			 * The context for the command batch is still valid -
			 * add it to the replay list
			 */
			replay[count++] = dispatcher->cmdqueue[ptr];
		} else {
			/*
			 * Skip over invaliated or detached contexts - cancel
			 * any pending events for the timestamp and destroy the
			 * command batch
			 */
			mutex_lock(&device->mutex);
			kgsl_cancel_events_timestamp(device, cmdbatch->context,
				cmdbatch->timestamp);
			mutex_unlock(&device->mutex);

			kgsl_cmdbatch_destroy(cmdbatch);
		}

		ptr = CMDQUEUE_NEXT(ptr, ADRENO_DISPATCH_CMDQUEUE_SIZE);
	}

	/* Reset the dispatcher queue */
	dispatcher->inflight = 0;
	dispatcher->head = dispatcher->tail = 0;

	mutex_lock(&device->mutex);
	BUG_ON(adreno_reset(device));
	mutex_unlock(&device->mutex);

	/* Replay the pending command buffers */
	for (i = 0; i < count; i++) {
		int ret = sendcmd(adreno_dev, replay[i]);

		/*
		 * I'm afraid that if we get an error during replay we
		 * are not going to space today
		 */

		BUG_ON(ret);
	}

	/*
	 * active_count will be set when we come into this function because
	 * there were inflight commands.  By virtue of setting ->inflight back
	 * to 0 sendcmd() will increase the active count again on the first
	 * submission.  This active_count_put is needed to put the universe back
	 * in balance and as a bonus it ensures that the hardware stays up for
	 * the entire reset process
	 */
	mutex_lock(&device->mutex);
	kgsl_active_count_put(device);
	mutex_unlock(&device->mutex);

	kfree(replay);
	return 0;
}

/**
 * adreno_dispatcher_queue_cmd() - Queue a new command in the context
 * @adreno_dev: Pointer to the adreno device struct
 * @drawctxt: Pointer to the adreno draw context
 * @cmdbatch: Pointer to the command batch being submitted
 * @timestamp: Pointer to the requested timestamp
 *
 * Queue a command in the context - if there isn't any room in the queue, then
 * block until there is
 */
int adreno_dispatcher_queue_cmd(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, struct kgsl_cmdbatch *cmdbatch,
		uint32_t *timestamp)
{
	int ret;

	mutex_lock(&drawctxt->mutex);

	if (drawctxt->flags & CTXT_FLAGS_BEING_DESTROYED) {
		mutex_unlock(&drawctxt->mutex);
		return -EINVAL;
	}

	/* Wait for room in the context queue */

	while (drawctxt->queued >= _context_cmdqueue_size) {
		trace_adreno_drawctxt_sleep(drawctxt);
		mutex_unlock(&drawctxt->mutex);

		ret = wait_event_interruptible_timeout(drawctxt->wq,
			_check_context_queue(drawctxt),
			msecs_to_jiffies(_context_queue_wait));

		mutex_lock(&drawctxt->mutex);
		trace_adreno_drawctxt_wake(drawctxt);

		if (ret <= 0) {
			mutex_unlock(&drawctxt->mutex);
			return (ret == 0) ? -ETIMEDOUT : (int) ret;
		}

		/*
		 * Account for the possiblity that the context got invalidated
		 * while we were sleeping
		 */

		if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID) {
			mutex_unlock(&drawctxt->mutex);
			return -EDEADLK;
		}
	}

	/*
	 * If the UMD specified a timestamp then use that under the condition
	 * that it is greater then the last queued timestamp in the context.
	 */

	if (drawctxt->flags & CTXT_FLAGS_USER_GENERATED_TS) {
		if (timestamp_cmp(drawctxt->timestamp, *timestamp) >= 0) {
			mutex_unlock(&drawctxt->mutex);
			return -ERANGE;
		}

		drawctxt->timestamp = *timestamp;
	} else
		drawctxt->timestamp++;

	cmdbatch->timestamp = drawctxt->timestamp;
	*timestamp = drawctxt->timestamp;

	/* Put the command into the queue */
	drawctxt->cmdqueue[drawctxt->cmdqueue_tail] = cmdbatch;
	drawctxt->cmdqueue_tail = (drawctxt->cmdqueue_tail + 1) %
		ADRENO_CONTEXT_CMDQUEUE_SIZE;

	drawctxt->queued++;
	trace_adreno_cmdbatch_queued(cmdbatch, drawctxt->queued);


	mutex_unlock(&drawctxt->mutex);

	/* Add the context to the dispatcher pending list */
	dispatcher_queue_context(adreno_dev, drawctxt);

	/*
	 * Only issue commands if inflight is less than burst -this prevents us
	 * from sitting around waiting for the mutex on a busy system - the work
	 * loop will schedule it for us. Inflight is mutex protected but the
	 * worse that can happen is that it will go to 0 after we check and if
	 * it goes to 0 it is because the work loop decremented it and the work
	 * queue will try to schedule new commands anyway.
	 */

	if (adreno_dev->dispatcher.inflight < _context_cmdbatch_burst)
		adreno_dispatcher_issuecmds(adreno_dev);

	return 0;
}

/**
 * dispatcher_do_fault() - Handle a GPU fault and reset the GPU
 * @device: Pointer to the KGSL device
 * @cmdbatch: Pointer to the command batch believed to be responsible for the
 * fault
 * @invalidate: Non zero if the current command should be invalidated
 *
 * Trigger a fault in the dispatcher and start the replay process
 */
static void dispatcher_do_fault(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch, int invalidate)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	unsigned int reg;

	/* Stop the timers */
	del_timer_sync(&dispatcher->timer);

	mutex_lock(&device->mutex);

	/*
	 * There is an interesting race condition here - when a command batch
	 * expires and we invaliate before we recover we run the risk of having
	 * the UMD clean up the context and free memory that the GPU is still
	 * using.  Not that it is dangerous because we are a few microseconds
	 * away from resetting, but it still ends up in pagefaults and log
	 * messages and so on and so forth. To avoid this we mark the command
	 * batch itself as invalid and then reset - the context will get
	 * invalidated in the replay.
	 */

	if (invalidate)
		cmdbatch->invalid = 1;

	/*
	 * Stop the CP in its tracks - this ensures that we don't get activity
	 * while we are trying to dump the state of the system
	 */


	adreno_readreg(adreno_dev, ADRENO_REG_CP_ME_CNTL, &reg);
	reg |= (1 << 27) | (1 << 28);
	adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, reg);

	kgsl_postmortem_dump(device, 0);
	kgsl_device_snapshot(device, 1);
	mutex_unlock(&device->mutex);

	/* If we can't replay then bravely run away and die */
	if (adreno_dispatcher_replay(adreno_dev))
		BUG();
}

static inline int cmdbatch_consumed(struct kgsl_cmdbatch *cmdbatch,
		unsigned int consumed, unsigned int retired)
{
	return ((timestamp_cmp(cmdbatch->timestamp, consumed) >= 0) &&
		(timestamp_cmp(retired, cmdbatch->timestamp) < 0));
}

/**
 * adreno_dispatcher_work() - Master work handler for the dispatcher
 * @work: Pointer to the work struct for the current work queue
 *
 * Process expired commands and send new ones.
 */
static void adreno_dispatcher_work(struct work_struct *work)
{
	struct adreno_dispatcher *dispatcher =
		container_of(work, struct adreno_dispatcher, work);
	struct adreno_device *adreno_dev =
		container_of(dispatcher, struct adreno_device, dispatcher);
	struct kgsl_device *device = &adreno_dev->dev;
	int inv, count = 0;

	mutex_lock(&dispatcher->mutex);

	while (dispatcher->head != dispatcher->tail) {
		uint32_t consumed, retired = 0;
		struct kgsl_cmdbatch *cmdbatch =
			dispatcher->cmdqueue[dispatcher->head];
		struct adreno_context *drawctxt;
		BUG_ON(cmdbatch == NULL);

		drawctxt = ADRENO_CONTEXT(cmdbatch->context);

		/*
		 * First try to expire the timestamp. This happens if the
		 * context is valid and the timestamp expired normally or if the
		 * context was destroyed before the command batch was finished
		 * in the GPU.  Either way retire the command batch advance the
		 * pointers and continue processing the queue
		 */

		if (!kgsl_context_detached(cmdbatch->context))
			retired = kgsl_readtimestamp(device, cmdbatch->context,
				KGSL_TIMESTAMP_RETIRED);

		if (kgsl_context_detached(cmdbatch->context) ||
			(timestamp_cmp(cmdbatch->timestamp, retired) <= 0)) {

			trace_adreno_cmdbatch_retired(cmdbatch,
				dispatcher->inflight - 1);

			/* Reduce the number of inflight command batches */
			dispatcher->inflight--;

			/* Zero the old entry*/
			dispatcher->cmdqueue[dispatcher->head] = NULL;

			/* Advance the buffer head */
			dispatcher->head = CMDQUEUE_NEXT(dispatcher->head,
				ADRENO_DISPATCH_CMDQUEUE_SIZE);

			/* Destroy the retired command batch */
			kgsl_cmdbatch_destroy(cmdbatch);

			/* Update the expire time for the next command batch */

			if (dispatcher->inflight > 0) {
				cmdbatch =
					dispatcher->cmdqueue[dispatcher->head];
				cmdbatch->expires = jiffies +
					msecs_to_jiffies(_cmdbatch_timeout);
			}

			count++;

			BUG_ON(dispatcher->inflight == 0 && dispatcher->fault);
			continue;
		}

		/*
		 * If we got a fault from the interrupt handler, this command
		 * is to blame.  Invalidate it, reset and replay
		 */

		if (dispatcher->fault) {
			dispatcher_do_fault(device, cmdbatch, 1);
			goto done;
		}

		/* Get the last consumed timestamp */
		consumed = kgsl_readtimestamp(device, cmdbatch->context,
			KGSL_TIMESTAMP_CONSUMED);

		/* Break here if fault detection is disabled for the context */
		if (drawctxt->flags & CTXT_FLAGS_NO_FAULT_TOLERANCE)
			break;

		/*
		 * The last line of defense is to check if the command batch has
		 * timed out. If we get this far but the timeout hasn't expired
		 * yet then the GPU is still ticking away
		 */

		if (time_is_after_jiffies(cmdbatch->expires))
			break;

		/* Boom goes the dynamite */

		pr_err("-----------------------\n");

		pr_err("dispatcher: expired ctx=%d ts=%d consumed=%d retired=%d\n",
			cmdbatch->context->id, cmdbatch->timestamp, consumed,
			retired);
		pr_err("dispatcher: jiffies=%lu expired=%lu\n", jiffies,
				cmdbatch->expires);

		/*
		 * If execution stopped after the current command batch was
		 * consumed then invalidate the context for the current command
		 * batch
		 */

		inv = cmdbatch_consumed(cmdbatch, consumed, retired);

		dispatcher_do_fault(device, cmdbatch, inv);
		break;
	}

	/*
	 * Decrement the active count to 0 - this will allow the system to go
	 * into suspend even if there are queued command batches
	 */

	if (count && dispatcher->inflight == 0) {
		mutex_lock(&device->mutex);
		kgsl_active_count_put(device);
		mutex_unlock(&device->mutex);
	}

	/* Dispatch new commands if we have the room */
	if (dispatcher->inflight < _dispatcher_inflight)
		_adreno_dispatcher_issuecmds(adreno_dev);

done:
	/* Either update the timer for the next command batch or disable it */
	if (dispatcher->inflight) {
		struct kgsl_cmdbatch *cmdbatch
			= dispatcher->cmdqueue[dispatcher->head];

		mod_timer(&dispatcher->timer, cmdbatch->expires);
	} else
		del_timer_sync(&dispatcher->timer);

	/* Before leaving update the pwrscale information */
	mutex_lock(&device->mutex);
	kgsl_pwrscale_idle(device);
	mutex_unlock(&device->mutex);

	mutex_unlock(&dispatcher->mutex);
}

void adreno_dispatcher_schedule(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	queue_work(device->work_queue, &dispatcher->work);
}

/*
 * This is called when the timer expires - it either means the GPU is hung or
 * the IB is taking too long to execute
 */
void adreno_dispatcher_timer(unsigned long data)
{
	struct adreno_device *adreno_dev = (struct adreno_device *) data;
	struct kgsl_device *device = &adreno_dev->dev;

	adreno_dispatcher_schedule(device);
}
/**
 * adreno_dispatcher_fault_irq() - Trigger a fault in the dispatcher
 * @device: Pointer to the KGSL device
 *
 * Called from an interrupt context this will trigger a fault in the
 * dispatcher
 */
void adreno_dispatcher_fault_irq(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	dispatcher->fault = 1;
	adreno_dispatcher_schedule(device);
}

/**
 * adreno_dispatcher_pause() - stop the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Pause the dispather so it doesn't accept any new commands
 */
void adreno_dispatcher_pause(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	/*
	 * This will probably get called while holding other mutexes so don't
	 * take the dispatcher mutex.  The biggest penalty is that another
	 * command might be submitted while we are in here but thats okay
	 * because whoever is waiting for the drain will just have another
	 * command batch to wait for
	 */

	dispatcher->state = ADRENO_DISPATCHER_PAUSE;
}

/**
 * adreno_dispatcher_start() - activate the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Set the disaptcher active and start the loop once to get things going
 */
void adreno_dispatcher_start(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	dispatcher->state = ADRENO_DISPATCHER_ACTIVE;

	/* Schedule the work loop to get things going */
	adreno_dispatcher_schedule(&adreno_dev->dev);
}

/**
 * adreno_dispatcher_stop() - stop the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Stop the dispatcher and close all the timers
 */
void adreno_dispatcher_stop(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	del_timer_sync(&dispatcher->timer);
	dispatcher->state = ADRENO_DISPATCHER_PAUSE;
}

/**
 * adreno_dispatcher_close() - close the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Close the dispatcher and free all the oustanding commands and memory
 */
void adreno_dispatcher_close(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	mutex_lock(&dispatcher->mutex);
	del_timer_sync(&dispatcher->timer);

	while (dispatcher->head != dispatcher->tail) {
		kgsl_cmdbatch_destroy(dispatcher->cmdqueue[dispatcher->head]);
		dispatcher->head = (dispatcher->head + 1)
			% ADRENO_DISPATCH_CMDQUEUE_SIZE;
	}

	mutex_unlock(&dispatcher->mutex);

	kobject_put(&dispatcher->kobj);
}

struct dispatcher_attribute {
	struct attribute attr;
	ssize_t (*show)(struct adreno_dispatcher *,
			struct dispatcher_attribute *, char *);
	ssize_t (*store)(struct adreno_dispatcher *,
			struct dispatcher_attribute *, const char *buf,
			size_t count);
	unsigned int max;
	unsigned int *value;
};

#define DISPATCHER_UINT_ATTR(_name, _mode, _max, _value) \
	struct dispatcher_attribute dispatcher_attr_##_name =  { \
		.attr = { .name = __stringify(_name), .mode = _mode }, \
		.show = _show_uint, \
		.store = _store_uint, \
		.max = _max, \
		.value = &(_value), \
	}

#define to_dispatcher_attr(_a) \
	container_of((_a), struct dispatcher_attribute, attr)
#define to_dispatcher(k) container_of(k, struct adreno_dispatcher, kobj)

static ssize_t _store_uint(struct adreno_dispatcher *dispatcher,
		struct dispatcher_attribute *attr,
		const char *buf, size_t size)
{
	unsigned long val;
	int ret = kstrtoul(buf, 0, &val);

	if (ret)
		return ret;

	if (!val || (attr->max && (val > attr->max)))
		return -EINVAL;

	*((unsigned int *) attr->value) = val;
	return size;
}

static ssize_t _show_uint(struct adreno_dispatcher *dispatcher,
		struct dispatcher_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
		*((unsigned int *) attr->value));
}

static DISPATCHER_UINT_ATTR(inflight, 0644, ADRENO_DISPATCH_CMDQUEUE_SIZE,
	_dispatcher_inflight);
/*
 * Our code that "puts back" a command from the context is much cleaner
 * if we are sure that there will always be enough room in the
 * ringbuffer so restrict the maximum size of the context queue to
 * ADRENO_CONTEXT_CMDQUEUE_SIZE - 1
 */
static DISPATCHER_UINT_ATTR(context_cmdqueue_size, 0644,
	ADRENO_CONTEXT_CMDQUEUE_SIZE - 1, _context_cmdqueue_size);
static DISPATCHER_UINT_ATTR(context_burst_count, 0644, 0,
	_context_cmdbatch_burst);
static DISPATCHER_UINT_ATTR(cmdbatch_timeout, 0644, 0, _cmdbatch_timeout);
static DISPATCHER_UINT_ATTR(context_queue_wait, 0644, 0, _context_queue_wait);

static struct attribute *dispatcher_attrs[] = {
	&dispatcher_attr_inflight.attr,
	&dispatcher_attr_context_cmdqueue_size.attr,
	&dispatcher_attr_context_burst_count.attr,
	&dispatcher_attr_cmdbatch_timeout.attr,
	&dispatcher_attr_context_queue_wait.attr,
	NULL,
};

static ssize_t dispatcher_sysfs_show(struct kobject *kobj,
				   struct attribute *attr, char *buf)
{
	struct adreno_dispatcher *dispatcher = to_dispatcher(kobj);
	struct dispatcher_attribute *pattr = to_dispatcher_attr(attr);
	ssize_t ret = -EIO;

	if (pattr->show)
		ret = pattr->show(dispatcher, pattr, buf);

	return ret;
}

static ssize_t dispatcher_sysfs_store(struct kobject *kobj,
				    struct attribute *attr,
				    const char *buf, size_t count)
{
	struct adreno_dispatcher *dispatcher = to_dispatcher(kobj);
	struct dispatcher_attribute *pattr = to_dispatcher_attr(attr);
	ssize_t ret = -EIO;

	if (pattr->store)
		ret = pattr->store(dispatcher, pattr, buf, count);

	return ret;
}

static void dispatcher_sysfs_release(struct kobject *kobj)
{
}

static const struct sysfs_ops dispatcher_sysfs_ops = {
	.show = dispatcher_sysfs_show,
	.store = dispatcher_sysfs_store
};

static struct kobj_type ktype_dispatcher = {
	.sysfs_ops = &dispatcher_sysfs_ops,
	.default_attrs = dispatcher_attrs,
	.release = dispatcher_sysfs_release
};

/**
 * adreno_dispatcher_init() - Initialize the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Initialize the dispatcher
 */
int adreno_dispatcher_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int ret;

	memset(dispatcher, 0, sizeof(*dispatcher));

	mutex_init(&dispatcher->mutex);

	setup_timer(&dispatcher->timer, adreno_dispatcher_timer,
		(unsigned long) adreno_dev);

	INIT_WORK(&dispatcher->work, adreno_dispatcher_work);

	plist_head_init(&dispatcher->pending);
	spin_lock_init(&dispatcher->plist_lock);

	dispatcher->state = ADRENO_DISPATCHER_ACTIVE;

	ret = kobject_init_and_add(&dispatcher->kobj, &ktype_dispatcher,
		&device->dev->kobj, "dispatch");

	return ret;
}
