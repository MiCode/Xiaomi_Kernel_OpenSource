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
#include <linux/err.h>

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

/* Interval for reading and comparing fault detection registers */
static unsigned int _fault_timer_interval = 50;

/* Local array for the current set of fault detect registers */
static unsigned int fault_detect_regs[FT_DETECT_REGS_COUNT];

/* The last retired global timestamp read during fault detect */
static unsigned int fault_detect_ts;

/**
 * fault_detect_read() - Read the set of fault detect registers
 * @device: Pointer to the KGSL device struct
 *
 * Read the set of fault detect registers and store them in the local array.
 * This is for the initial values that are compared later with
 * fault_detect_read_compare
 */
static void fault_detect_read(struct kgsl_device *device)
{
	int i;

	fault_detect_ts = kgsl_readtimestamp(device, NULL,
		KGSL_TIMESTAMP_RETIRED);

	for (i = 0; i < FT_DETECT_REGS_COUNT; i++) {
		if (ft_detect_regs[i] == 0)
			continue;
		kgsl_regread(device, ft_detect_regs[i],
			&fault_detect_regs[i]);
	}
}

/**
 * fault_detect_read_compare() - Read the fault detect registers and compare
 * them to the current value
 * @device: Pointer to the KGSL device struct
 *
 * Read the set of fault detect registers and compare them to the current set
 * of registers.  Return 1 if any of the register values changed
 */
static int fault_detect_read_compare(struct kgsl_device *device)
{
	int i, ret = 0;
	unsigned int ts;

	for (i = 0; i < FT_DETECT_REGS_COUNT; i++) {
		unsigned int val;

		if (ft_detect_regs[i] == 0)
			continue;
		kgsl_regread(device, ft_detect_regs[i], &val);
		if (val != fault_detect_regs[i])
			ret = 1;
		fault_detect_regs[i] = val;
	}

	ts = kgsl_readtimestamp(device, NULL, KGSL_TIMESTAMP_RETIRED);
	if (ts != fault_detect_ts)
		ret = 1;

	fault_detect_ts = ts;

	return ret;
}

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

		/*
		 * Don't dequeue a cmdbatch that is still waiting for other
		 * events
		 */
		if (kgsl_cmdbatch_sync_pending(cmdbatch)) {
			cmdbatch = ERR_PTR(-EAGAIN);
			goto done;
		}

		drawctxt->cmdqueue_head =
			CMDQUEUE_NEXT(drawctxt->cmdqueue_head,
			ADRENO_CONTEXT_CMDQUEUE_SIZE);
		drawctxt->queued--;
	}

done:
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

	/*
	 * On the first command, if the submission was successful, then read the
	 * fault registers.  If it failed then turn off the GPU. Sad face.
	 */

	if (dispatcher->inflight == 1) {
		if (ret == 0)
			fault_detect_read(device);
		else
			kgsl_active_count_put(device);
	}

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

		/* Start the fault detection timer */
		if (adreno_dev->fast_hang_detect)
			mod_timer(&dispatcher->fault_timer,
				jiffies +
				msecs_to_jiffies(_fault_timer_interval));
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
	int requeued = 0;

	/*
	 * Each context can send a specific number of command batches per cycle
	 */
	while ((count < _context_cmdbatch_burst) &&
		(dispatcher->inflight < _dispatcher_inflight)) {
		int ret;
		struct kgsl_cmdbatch *cmdbatch;

		if (dispatcher->state != ADRENO_DISPATCHER_ACTIVE)
			break;

		if (adreno_gpu_fault(adreno_dev) != 0)
			break;

		cmdbatch = adreno_dispatcher_get_cmdbatch(drawctxt);

		if (cmdbatch == NULL)
			break;

		/*
		 * adreno_context_get_cmdbatch returns -EAGAIN if the current
		 * cmdbatch has pending sync points so no more to do here.
		 * When the sync points are satisfied then the context will get
		 * reqeueued
		 */

		if (IS_ERR(cmdbatch) && PTR_ERR(cmdbatch) == -EAGAIN) {
			requeued = 1;
			break;
		}

		/*
		 * If this is a synchronization submission then there are no
		 * commands to submit.  Discard it and get the next item from
		 * the queue.  Decrement count so this packet doesn't count
		 * against the burst for the context
		 */

		if (cmdbatch->flags & KGSL_CONTEXT_SYNC) {
			kgsl_cmdbatch_destroy(cmdbatch);
			continue;
		}

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
			requeued = 1;
			break;
		}
		count++;
	}

	/*
	 * If the context successfully submitted commands, then
	 * unconditionally put it back on the queue to be considered the
	 * next time around. This might seem a little wasteful but it is
	 * reasonable to think that a busy context will stay busy.
	 */

	if (count || requeued) {
		dispatcher_queue_context(adreno_dev, drawctxt);

		/*
		 * If we submitted something there will be room in the
		 * context queue so ping the context wait queue on the
		 * chance that the context is snoozing
		 */

		wake_up_interruptible_all(&drawctxt->wq);
	}

	/* Return the number of command batches processed */
	if (count > 0)
		return count;

	/*
	 * If we didn't process anything because of a stall or an error
	 * return -1 so the issuecmds loop knows that we shouldn't
	 * keep trying to process it
	 */

	return requeued ? -1 : 0;
}

static void plist_move(struct plist_head *old, struct plist_head *new)
{
	plist_head_init(new);
	list_splice_tail(&old->node_list, &new->node_list);
	plist_head_init(old);
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
	struct plist_head tmp;
	struct adreno_context *drawctxt, *next;

	/* Leave early if the dispatcher isn't in a happy state */
	if ((dispatcher->state != ADRENO_DISPATCHER_ACTIVE) ||
		adreno_gpu_fault(adreno_dev) != 0)
			return 0;

	/* Copy the current context list to a temporary list */
	spin_lock(&dispatcher->plist_lock);
	plist_move(&dispatcher->pending, &tmp);
	spin_unlock(&dispatcher->plist_lock);

	/* Try to fill the ringbuffer as much as possible */
	while (dispatcher->inflight < _dispatcher_inflight) {

		/* Stop doing things if the dispatcher is paused or faulted */
		if ((dispatcher->state != ADRENO_DISPATCHER_ACTIVE) ||
			adreno_gpu_fault(adreno_dev) != 0)
			break;

		if (plist_head_empty(&tmp))
			break;

		/* Get the next entry on the list */
		drawctxt = plist_first_entry(&tmp, struct adreno_context,
			pending);

		/* Remove it from the list */
		plist_del(&drawctxt->pending, &tmp);

		if (kgsl_context_detached(&drawctxt->base) ||
			drawctxt->state == ADRENO_CONTEXT_STATE_INVALID) {
			kgsl_context_put(&drawctxt->base);
			continue;
		}

		dispatcher_context_sendcmds(adreno_dev, drawctxt);
		kgsl_context_put(&drawctxt->base);
	}

	/* Requeue any remaining contexts for the next go around */

	spin_lock(&dispatcher->plist_lock);

	plist_for_each_entry_safe(drawctxt, next, &tmp, pending) {
		int prio = drawctxt->pending.prio;

		/* Reset the context node */
		plist_node_init(&drawctxt->pending, prio);

		/* And put it back in the master list */
		plist_add(&drawctxt->pending, &dispatcher->pending);
	}

	spin_unlock(&dispatcher->plist_lock);

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
 * get_timestamp() - Return the next timestamp for the context
 * @drawctxt - Pointer to an adreno draw context struct
 * @cmdbatch - Pointer to a command batch
 * @timestamp - Pointer to a timestamp value possibly passed from the user
 *
 * Assign a timestamp based on the settings of the draw context and the command
 * batch.
 */
static int get_timestamp(struct adreno_context *drawctxt,
		struct kgsl_cmdbatch *cmdbatch, unsigned int *timestamp)
{
	/* Synchronization commands don't get a timestamp */
	if (cmdbatch->flags & KGSL_CONTEXT_SYNC) {
		*timestamp = 0;
		return 0;
	}

	if (drawctxt->flags & CTXT_FLAGS_USER_GENERATED_TS) {
		/*
		 * User specified timestamps need to be greater than the last
		 * issued timestamp in the context
		 */
		if (timestamp_cmp(drawctxt->timestamp, *timestamp) >= 0)
			return -ERANGE;

		drawctxt->timestamp = *timestamp;
	} else
		drawctxt->timestamp++;

	*timestamp = drawctxt->timestamp;
	return 0;
}

/**
 * adreno_dispactcher_queue_cmd() - Queue a new command in the context
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

	/*
	 * After skipping to the end of the frame we need to force the preamble
	 * to run (if it exists) regardless of the context state.
	 */

	if (drawctxt->flags & CTXT_FLAGS_FORCE_PREAMBLE) {
		set_bit(CMDBATCH_FLAG_FORCE_PREAMBLE, &cmdbatch->priv);
		drawctxt->flags &= ~CTXT_FLAGS_FORCE_PREAMBLE;
	}

	/*
	 * If we are waiting for the end of frame and it hasn't appeared yet,
	 * then mark the command batch as skipped.  It will still progress
	 * through the pipeline but it won't actually send any commands
	 */

	if (drawctxt->flags & CTXT_FLAGS_SKIP_EOF) {
		set_bit(CMDBATCH_FLAG_SKIP, &cmdbatch->priv);

		/*
		 * If this command batch represents the EOF then clear the way
		 * for the dispatcher to continue submitting
		 */

		if (cmdbatch->flags & KGSL_CONTEXT_END_OF_FRAME) {
			drawctxt->flags &= ~CTXT_FLAGS_SKIP_EOF;

			/*
			 * Force the preamble on the next command to ensure that
			 * the state is correct
			 */

			drawctxt->flags |= CTXT_FLAGS_FORCE_PREAMBLE;
		}
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

	ret = get_timestamp(drawctxt, cmdbatch, timestamp);
	if (ret) {
		mutex_unlock(&drawctxt->mutex);
		return ret;
	}

	cmdbatch->timestamp = *timestamp;

	/*
	 * Set the fault tolerance policy for the command batch - assuming the
	 * context hsn't disabled FT use the current device policy
	 */

	if (drawctxt->flags & CTXT_FLAGS_NO_FAULT_TOLERANCE)
		set_bit(KGSL_FT_DISABLE, &cmdbatch->fault_policy);
	else
		cmdbatch->fault_policy = adreno_dev->ft_policy;

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

/*
 * If an IB inside of the command batch has a gpuaddr that matches the base
 * passed in then zero the size which effectively skips it when it is submitted
 * in the ringbuffer.
 */
static void cmdbatch_skip_ib(struct kgsl_cmdbatch *cmdbatch, unsigned int base)
{
	int i;

	for (i = 0; i < cmdbatch->ibcount; i++) {
		if (cmdbatch->ibdesc[i].gpuaddr == base) {
			cmdbatch->ibdesc[i].sizedwords = 0;
			return;
		}
	}
}

static void cmdbatch_skip_frame(struct kgsl_cmdbatch *cmdbatch,
	struct kgsl_cmdbatch **replay, int count)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(cmdbatch->context);
	int skip = 1;
	int i;

	for (i = 0; i < count; i++) {

		/*
		 * Only operate on command batches that belong to the
		 * faulting context
		 */

		if (replay[i]->context->id != cmdbatch->context->id)
			continue;

		/*
		 * Skip all the command batches in this context until
		 * the EOF flag is seen.  If the EOF flag is seen then
		 * force the preamble for the next command.
		 */

		if (skip) {
			set_bit(CMDBATCH_FLAG_SKIP, &replay[i]->priv);

			if (replay[i]->flags & KGSL_CONTEXT_END_OF_FRAME)
				skip = 0;
		} else {
			set_bit(CMDBATCH_FLAG_FORCE_PREAMBLE, &replay[i]->priv);
			return;
		}
	}

	/*
	 * If the EOF flag hasn't been seen yet then set the flag in the
	 * drawctxt to keep looking for it
	 */

	if (skip && drawctxt)
		drawctxt->flags |= CTXT_FLAGS_SKIP_EOF;

	/*
	 * If we did see the EOF flag then force the preamble on for the
	 * next command issued on this context
	 */

	if (!skip && drawctxt)
		drawctxt->flags |= CTXT_FLAGS_FORCE_PREAMBLE;
}

static void remove_invalidated_cmdbatches(struct kgsl_device *device,
		struct kgsl_cmdbatch **replay, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		struct kgsl_cmdbatch *cmd = replay[i];
		struct adreno_context *drawctxt;

		if (cmd == NULL)
			continue;

		drawctxt = ADRENO_CONTEXT(cmd->context);

		if (kgsl_context_detached(cmd->context) ||
			drawctxt->state == ADRENO_CONTEXT_STATE_INVALID) {
			replay[i] = NULL;

			mutex_lock(&device->mutex);
			kgsl_cancel_events_timestamp(device, cmd->context,
				cmd->timestamp);
			mutex_unlock(&device->mutex);

			kgsl_cmdbatch_destroy(cmd);
		}
	}
}

static char _pidname[TASK_COMM_LEN];

static inline const char *_kgsl_context_comm(struct kgsl_context *context)
{
	struct task_struct *task = NULL;

	if (context)
		task = find_task_by_vpid(context->pid);

	if (task)
		get_task_comm(_pidname, task);
	else
		snprintf(_pidname, TASK_COMM_LEN, "unknown");

	return _pidname;
}

#define pr_fault(_d, _c, fmt, args...) \
		dev_err((_d)->dev, "%s[%d]: " fmt, \
		_kgsl_context_comm((_c)->context), \
		(_c)->context->pid, ##args)


static void adreno_fault_header(struct kgsl_device *device,
	struct kgsl_cmdbatch *cmdbatch)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int status, base, rptr, wptr, ib1base, ib2base, ib1sz, ib2sz;

	kgsl_regread(device,
			adreno_getreg(adreno_dev, ADRENO_REG_RBBM_STATUS),
			&status);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_RB_BASE),
		&base);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_RB_RPTR),
		&rptr);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_RB_WPTR),
		&wptr);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_IB1_BASE),
		&ib1base);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_IB1_BUFSZ),
		&ib1sz);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_IB2_BASE),
		&ib2base);
	kgsl_regread(device,
		adreno_getreg(adreno_dev, ADRENO_REG_CP_IB2_BUFSZ),
		&ib2sz);

	trace_adreno_gpu_fault(cmdbatch->context->id, cmdbatch->timestamp,
		status, rptr, wptr, ib1base, ib1sz, ib2base, ib2sz);

	pr_fault(device, cmdbatch,
		"gpu fault ctx %d ts %d status %8.8X rb %4.4x/%4.4x ib1 %8.8x/%4.4x ib2 %8.8x/%4.4x\n",
		cmdbatch->context->id, cmdbatch->timestamp, status,
		rptr, wptr, ib1base, ib1sz, ib2base, ib2sz);
}

static int dispatcher_do_fault(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	unsigned int ptr;
	unsigned int reg, base;
	struct kgsl_cmdbatch **replay = NULL;
	struct kgsl_cmdbatch *cmdbatch;
	int ret, i, count = 0;
	int fault, first = 0;
	bool pagefault = false;
	BUG_ON(dispatcher->inflight == 0);

	fault = atomic_xchg(&dispatcher->fault, 0);
	if (fault == 0)
		return 0;

	/* Turn off all the timers */
	del_timer_sync(&dispatcher->timer);
	del_timer_sync(&dispatcher->fault_timer);

	mutex_lock(&device->mutex);

	cmdbatch = dispatcher->cmdqueue[dispatcher->head];

	trace_adreno_cmdbatch_fault(cmdbatch, fault);

	/*
	 * If the fault was due to a timeout then stop the CP to ensure we don't
	 * get activity while we are trying to dump the state of the system
	 */

	if (fault == ADRENO_TIMEOUT_FAULT) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_ME_CNTL, &reg);
		reg |= (1 << 27) | (1 << 28);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, reg);

		/* Skip the PM dump for a timeout because it confuses people */
		set_bit(KGSL_FT_SKIP_PMDUMP, &cmdbatch->fault_policy);
	}

	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BASE, &base);

	/*
	 * Dump the postmortem and snapshot information if this is the first
	 * detected fault for the oldest active command batch
	 */

	if (!test_bit(KGSL_FT_SKIP_PMDUMP, &cmdbatch->fault_policy)) {
		adreno_fault_header(device, cmdbatch);

		if (device->pm_dump_enable)
			kgsl_postmortem_dump(device, 0);

		kgsl_device_snapshot(device, 1);
	}

	mutex_unlock(&device->mutex);

	/* Allocate memory to store the inflight commands */
	replay = kzalloc(sizeof(*replay) * dispatcher->inflight, GFP_KERNEL);

	if (replay == NULL) {
		unsigned int ptr = dispatcher->head;

		while (ptr != dispatcher->tail) {
			struct kgsl_context *context =
				dispatcher->cmdqueue[ptr]->context;

			adreno_drawctxt_invalidate(device, context);
			kgsl_cmdbatch_destroy(dispatcher->cmdqueue[ptr]);

			ptr = CMDQUEUE_NEXT(ptr, ADRENO_DISPATCH_CMDQUEUE_SIZE);
		}

		/*
		 * Set the replay count to zero - this will ensure that the
		 * hardware gets reset but nothing else goes played
		 */

		count = 0;
		goto replay;
	}

	/* Copy the inflight command batches into the temporary storage */
	ptr = dispatcher->head;

	while (ptr != dispatcher->tail) {
		replay[count++] = dispatcher->cmdqueue[ptr];
		ptr = CMDQUEUE_NEXT(ptr, ADRENO_DISPATCH_CMDQUEUE_SIZE);
	}

	/*
	 * For the purposes of replay, we assume that the oldest command batch
	 * that hasn't retired a timestamp is "hung".
	 */

	cmdbatch = replay[0];

	/*
	 * If FT is disabled for this cmdbatch invalidate immediately
	 */

	if (test_bit(KGSL_FT_DISABLE, &cmdbatch->fault_policy) ||
		test_bit(KGSL_FT_TEMP_DISABLE, &cmdbatch->fault_policy)) {
		pr_fault(device, cmdbatch, "gpu skipped ctx %d ts %d\n",
			cmdbatch->context->id, cmdbatch->timestamp);

		adreno_drawctxt_invalidate(device, cmdbatch->context);
	}

	/*
	 * Set a flag so we don't print another PM dump if the cmdbatch fails
	 * again on replay
	 */

	set_bit(KGSL_FT_SKIP_PMDUMP, &cmdbatch->fault_policy);

	/*
	 * A hardware fault generally means something was deterministically
	 * wrong with the command batch - no point in trying to replay it
	 * Clear the replay bit and move on to the next policy level
	 */

	if (fault == ADRENO_HARD_FAULT)
		clear_bit(KGSL_FT_REPLAY, &(cmdbatch->fault_policy));

	/*
	 * A timeout fault means the IB timed out - clear the policy and
	 * invalidate - this will clear the FT_SKIP_PMDUMP bit but that is okay
	 * because we won't see this cmdbatch again
	 */

	if (fault == ADRENO_TIMEOUT_FAULT)
		bitmap_zero(&cmdbatch->fault_policy, BITS_PER_LONG);

	/*
	 * If the context had a GPU page fault then it is likely it would fault
	 * again if replayed
	 */

	if (test_bit(KGSL_CONTEXT_PAGEFAULT, &cmdbatch->context->priv)) {
		/* we'll need to resume the mmu later... */
		pagefault = true;
		clear_bit(KGSL_FT_REPLAY, &cmdbatch->fault_policy);
		clear_bit(KGSL_CONTEXT_PAGEFAULT, &cmdbatch->context->priv);
	}

	/*
	 * Execute the fault tolerance policy. Each command batch stores the
	 * current fault policy that was set when it was queued.
	 * As the options are tried in descending priority
	 * (REPLAY -> SKIPIBS -> SKIPFRAME -> NOTHING) the bits are cleared
	 * from the cmdbatch policy so the next thing can be tried if the
	 * change comes around again
	 */

	/* Replay the hanging command batch again */
	if (test_and_clear_bit(KGSL_FT_REPLAY, &cmdbatch->fault_policy)) {
		trace_adreno_cmdbatch_recovery(cmdbatch, BIT(KGSL_FT_REPLAY));
		set_bit(KGSL_FT_REPLAY, &cmdbatch->fault_recovery);
		goto replay;
	}

	/*
	 * Skip the last IB1 that was played but replay everything else.
	 * Note that the last IB1 might not be in the "hung" command batch
	 * because the CP may have caused a page-fault while it was prefetching
	 * the next IB1/IB2. walk all outstanding commands and zap the
	 * supposedly bad IB1 where ever it lurks.
	 */

	if (test_and_clear_bit(KGSL_FT_SKIPIB, &cmdbatch->fault_policy)) {
		trace_adreno_cmdbatch_recovery(cmdbatch, BIT(KGSL_FT_SKIPIB));
		set_bit(KGSL_FT_SKIPIB, &cmdbatch->fault_recovery);

		for (i = 0; i < count; i++) {
			if (replay[i] != NULL)
				cmdbatch_skip_ib(replay[i], base);
		}

		goto replay;
	}

	if (test_and_clear_bit(KGSL_FT_SKIPFRAME, &cmdbatch->fault_policy)) {
		trace_adreno_cmdbatch_recovery(cmdbatch,
			BIT(KGSL_FT_SKIPFRAME));
		set_bit(KGSL_FT_SKIPFRAME, &cmdbatch->fault_recovery);

		/*
		 * Skip all the pending command batches for this context until
		 * the EOF frame is seen
		 */
		cmdbatch_skip_frame(cmdbatch, replay, count);
		goto replay;
	}

	/* If we get here then all the policies failed */

	pr_fault(device, cmdbatch, "gpu failed ctx %d ts %d\n",
		cmdbatch->context->id, cmdbatch->timestamp);

	/* Invalidate the context */
	adreno_drawctxt_invalidate(device, cmdbatch->context);


replay:
	/* Reset the dispatcher queue */
	dispatcher->inflight = 0;
	dispatcher->head = dispatcher->tail = 0;

	/* Reset the GPU */
	mutex_lock(&device->mutex);

	/* resume the MMU if it is stalled */
	if (pagefault && device->mmu.mmu_ops->mmu_pagefault_resume != NULL)
		device->mmu.mmu_ops->mmu_pagefault_resume(&device->mmu);

	ret = adreno_reset(device);
	mutex_unlock(&device->mutex);

	/* If adreno_reset() fails then what hope do we have for the future? */
	BUG_ON(ret);

	/* Remove any pending command batches that have been invalidated */
	remove_invalidated_cmdbatches(device, replay, count);

	/* Replay the pending command buffers */
	for (i = 0; i < count; i++) {

		int ret;

		if (replay[i] == NULL)
			continue;

		/*
		 * Force the preamble on the first command (if applicable) to
		 * avoid any strange stage issues
		 */

		if (first == 0) {
			set_bit(CMDBATCH_FLAG_FORCE_PREAMBLE, &replay[i]->priv);
			first = 1;
		}

		/*
		 * Force each command batch to wait for idle - this avoids weird
		 * CP parse issues
		 */

		set_bit(CMDBATCH_FLAG_WFI, &replay[i]->priv);

		ret = sendcmd(adreno_dev, replay[i]);

		/*
		 * If sending the command fails, then try to recover by
		 * invalidating the context
		 */

		if (ret) {
			pr_fault(device, replay[i],
				"gpu reset failed ctx %d ts %d\n",
				replay[i]->context->id, replay[i]->timestamp);

			adreno_drawctxt_invalidate(device, replay[i]->context);
			remove_invalidated_cmdbatches(device, &replay[i],
				count - i);
		}
	}

	mutex_lock(&device->mutex);
	kgsl_active_count_put(device);
	mutex_unlock(&device->mutex);

	kfree(replay);

	return 1;
}

static inline int cmdbatch_consumed(struct kgsl_cmdbatch *cmdbatch,
		unsigned int consumed, unsigned int retired)
{
	return ((timestamp_cmp(cmdbatch->timestamp, consumed) >= 0) &&
		(timestamp_cmp(retired, cmdbatch->timestamp) < 0));
}

static void _print_recovery(struct kgsl_device *device,
		struct kgsl_cmdbatch *cmdbatch)
{
	static struct {
		unsigned int mask;
		const char *str;
	} flags[] = { ADRENO_FT_TYPES };

	int i, nr = find_first_bit(&cmdbatch->fault_recovery, BITS_PER_LONG);
	char *result = "unknown";

	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		if (flags[i].mask == BIT(nr)) {
			result = (char *) flags[i].str;
			break;
		}
	}

	pr_fault(device, cmdbatch,
		"gpu %s ctx %d ts %d policy %lX\n",
		result, cmdbatch->context->id, cmdbatch->timestamp,
		cmdbatch->fault_recovery);
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
	int count = 0;

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

			/*
			 * If the cmdbatch in question had faulted announce its
			 * successful completion to the world
			 */

			if (cmdbatch->fault_recovery != 0)
				_print_recovery(device, cmdbatch);

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
			continue;
		}

		/*
		 * If we got a fault from the interrupt handler, this command
		 * is to blame.  Invalidate it, reset and replay
		 */

		if (dispatcher_do_fault(device))
			goto done;

		/* Get the last consumed timestamp */
		consumed = kgsl_readtimestamp(device, cmdbatch->context,
			KGSL_TIMESTAMP_CONSUMED);

		/*
		 * Break here if fault detection is disabled for the context or
		 * if the long running IB detection is disaled device wide
		 * Long running command buffers will be allowed to run to
		 * completion - but badly behaving command buffers (infinite
		 * shaders etc) can end up running forever.
		 */

		if (!adreno_dev->long_ib_detect ||
			drawctxt->flags & CTXT_FLAGS_NO_FAULT_TOLERANCE)
			break;

		/*
		 * The last line of defense is to check if the command batch has
		 * timed out. If we get this far but the timeout hasn't expired
		 * yet then the GPU is still ticking away
		 */

		if (time_is_after_jiffies(cmdbatch->expires))
			break;

		/* Boom goes the dynamite */

		pr_fault(device, cmdbatch,
			"gpu timeout ctx %d ts %d\n",
			cmdbatch->context->id, cmdbatch->timestamp);

		adreno_set_gpu_fault(adreno_dev, ADRENO_TIMEOUT_FAULT);

		dispatcher_do_fault(device);
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

		/* Update the timeout timer for the next command batch */
		mod_timer(&dispatcher->timer, cmdbatch->expires);
	} else {
		del_timer_sync(&dispatcher->timer);
		del_timer_sync(&dispatcher->fault_timer);
	}

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

/**
 * adreno_dispatcher_queue_context() - schedule a drawctxt in the dispatcher
 * device: pointer to the KGSL device
 * drawctxt: pointer to the drawctxt to schedule
 *
 * Put a draw context on the dispatcher pending queue and schedule the
 * dispatcher. This is used to reschedule changes that might have been blocked
 * for sync points or other concerns
 */
void adreno_dispatcher_queue_context(struct kgsl_device *device,
	struct adreno_context *drawctxt)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	dispatcher_queue_context(adreno_dev, drawctxt);
	adreno_dispatcher_schedule(device);
}

/*
 * This is called on a regular basis while command batches are inflight.  Fault
 * detection registers are read and compared to the existing values - if they
 * changed then the GPU is still running.  If they are the same between
 * subsequent calls then the GPU may have faulted
 */

void adreno_dispatcher_fault_timer(unsigned long data)
{
	struct adreno_device *adreno_dev = (struct adreno_device *) data;
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	/* Leave if the user decided to turn off fast hang detection */
	if (adreno_dev->fast_hang_detect == 0)
		return;

	if (adreno_gpu_fault(adreno_dev)) {
		adreno_dispatcher_schedule(device);
		return;
	}

	/*
	 * Read the fault registers - if it returns 0 then they haven't changed
	 * so mark the dispatcher as faulted and schedule the work loop.
	 */

	if (!fault_detect_read_compare(device)) {
		adreno_set_gpu_fault(adreno_dev, ADRENO_SOFT_FAULT);
		adreno_dispatcher_schedule(device);
	} else {
		mod_timer(&dispatcher->fault_timer,
			jiffies + msecs_to_jiffies(_fault_timer_interval));
	}
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
 * adreno_dispatcher_irq_fault() - Trigger a fault in the dispatcher
 * @device: Pointer to the KGSL device
 *
 * Called from an interrupt context this will trigger a fault in the
 * dispatcher for the oldest pending command batch
 */
void adreno_dispatcher_irq_fault(struct kgsl_device *device)
{
	adreno_set_gpu_fault(ADRENO_DEVICE(device), ADRENO_HARD_FAULT);
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
	del_timer_sync(&dispatcher->fault_timer);

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
	del_timer_sync(&dispatcher->fault_timer);

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
static DISPATCHER_UINT_ATTR(fault_detect_interval, 0644, 0,
	_fault_timer_interval);

static struct attribute *dispatcher_attrs[] = {
	&dispatcher_attr_inflight.attr,
	&dispatcher_attr_context_cmdqueue_size.attr,
	&dispatcher_attr_context_burst_count.attr,
	&dispatcher_attr_cmdbatch_timeout.attr,
	&dispatcher_attr_context_queue_wait.attr,
	&dispatcher_attr_fault_detect_interval.attr,
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

	setup_timer(&dispatcher->fault_timer, adreno_dispatcher_fault_timer,
		(unsigned long) adreno_dev);

	INIT_WORK(&dispatcher->work, adreno_dispatcher_work);

	plist_head_init(&dispatcher->pending);
	spin_lock_init(&dispatcher->plist_lock);

	dispatcher->state = ADRENO_DISPATCHER_ACTIVE;

	ret = kobject_init_and_add(&dispatcher->kobj, &ktype_dispatcher,
		&device->dev->kobj, "dispatch");

	return ret;
}
